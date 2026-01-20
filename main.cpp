// main.cpp  (SFML 2.6.x compatible, Fedora-friendly)
// Drop-in replacement for your entire file.

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

static sf::Vector2f snap(sf::Vector2f p) { return sf::Vector2f(std::round(p.x), std::round(p.y)); }

// ======================== Squares / Coords ========================
struct Square { int file=0, rank=0; }; // 0..7
static bool operator==(const Square& a, const Square& b){ return a.file==b.file && a.rank==b.rank; }
static bool inBounds(const Square& s){ return s.file>=0 && s.file<8 && s.rank>=0 && s.rank<8; }
static int sqToIndex(const Square& s){ return s.rank*8 + s.file; }
static Square indexToSq(int idx){ return Square{idx%8, idx/8}; }
static std::string sqName(const Square& s){
    return std::string() + char('a'+s.file) + char('1'+s.rank);
}

// Visual board: rank 7 at top visually unless flipped
static sf::Vector2f squareToPixel(const Square& s, float tile, sf::Vector2f origin, bool flip){
    int vr = flip ? s.rank : (7 - s.rank);
    int vf = flip ? (7 - s.file) : s.file;
    return sf::Vector2f(origin.x + vf*tile, origin.y + vr*tile);
}
static std::optional<Square> pixelToSquare(sf::Vector2f p, float tile, sf::Vector2f origin, bool flip){
    float x=p.x-origin.x, y=p.y-origin.y;
    if(x<0||y<0) return std::nullopt;
    int vf=int(x/tile), vr=int(y/tile);
    if(vf<0||vf>7||vr<0||vr>7) return std::nullopt;
    int file = flip ? (7 - vf) : vf;
    int rank = flip ? vr : (7 - vr);
    return Square{file, rank};
}

static sf::Color lighten(sf::Color c, int add){
    auto clamp=[](int v){ return std::max(0,std::min(255,v)); };
    return sf::Color(
        static_cast<sf::Uint8>(clamp(int(c.r)+add)),
        static_cast<sf::Uint8>(clamp(int(c.g)+add)),
        static_cast<sf::Uint8>(clamp(int(c.b)+add)),
        c.a
    );
}

// ======================== Chess Types ========================
enum class Color : u8 { White=0, Black=1 };
static Color other(Color c){ return c==Color::White ? Color::Black : Color::White; }

enum class PieceType : u8 { None=0, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType t = PieceType::None;
    Color c = Color::White;
};
static bool isNone(const Piece& p){ return p.t==PieceType::None; }

static int pieceValue(PieceType t){
    switch(t){
        case PieceType::Pawn:   return 100;
        case PieceType::Knight: return 320;
        case PieceType::Bishop: return 330;
        case PieceType::Rook:   return 500;
        case PieceType::Queen:  return 900;
        case PieceType::King:   return 0;
        default: return 0;
    }
}

static std::string pieceName(PieceType t){
    switch(t){
        case PieceType::Pawn:   return "pawn";
        case PieceType::Knight: return "knight";
        case PieceType::Bishop: return "bishop";
        case PieceType::Rook:   return "rook";
        case PieceType::Queen:  return "queen";
        case PieceType::King:   return "king";
        default: return "";
    }
}
static std::string pieceKey(const Piece& p){
    if(p.t==PieceType::None) return "";
    std::string col = (p.c==Color::White) ? "white_" : "black_";
    return col + pieceName(p.t);
}

struct Move {
    u8 from=0, to=0;
    PieceType promo = PieceType::None;
    bool isCapture=false;
    bool isEnPassant=false;
    bool isCastle=false;
};

struct Undo {
    Move m{};
    Piece captured{};
    int epSquare=-1;
    u8 castling=0;
    int halfmoveClock=0;
    u64 hash=0;
};

static std::string moveToUCI(const Move& m){
    Square a = indexToSq(m.from);
    Square b = indexToSq(m.to);
    std::string s = sqName(a) + sqName(b);
    if(m.promo!=PieceType::None){
        char pc='q';
        if(m.promo==PieceType::Rook) pc='r';
        if(m.promo==PieceType::Bishop) pc='b';
        if(m.promo==PieceType::Knight) pc='n';
        s.push_back(pc);
    }
    return s;
}

// ======================== Zobrist + TT ========================
struct Zobrist {
    // [color][pieceType][square]
    u64 psq[2][7][64]{};
    u64 sideToMove{};
    u64 castling[16]{};
    u64 epFile[9]{}; // 0..7 file, 8 = "no ep"

    Zobrist(){
        std::mt19937_64 rng(0xC0FFEE1234ULL);
        auto r64 = [&](){ return rng(); };

        for(int c=0;c<2;c++)
            for(int pt=0;pt<7;pt++)
                for(int s=0;s<64;s++)
                    psq[c][pt][s]=r64();

        sideToMove = r64();
        for(int i=0;i<16;i++) castling[i]=r64();
        for(int i=0;i<9;i++) epFile[i]=r64();
    }
};

enum class TTFlag : u8 { Exact=0, Lower=1, Upper=2 };

struct TTEntry {
    u64 key=0;
    int16_t score=0;
    int8_t depth=0;
    TTFlag flag=TTFlag::Exact;
    Move best{};
};

struct TranspositionTable {
    std::vector<TTEntry> table;
    size_t mask=0;

    void resizeMB(size_t mb){
        size_t bytes = mb*1024ull*1024ull;
        size_t n = std::max<size_t>(1, bytes / sizeof(TTEntry));
        size_t p=1;
        while(p < n) p<<=1;
        table.assign(p, TTEntry{});
        mask = p-1;
    }

    TTEntry* probe(u64 key){
        if(table.empty()) return nullptr;
        return &table[size_t(key) & mask];
    }

    void store(u64 key, int depth, int score, TTFlag flag, const Move& best){
        if(table.empty()) return;
        TTEntry& e = table[size_t(key) & mask];
        if(e.key==0 || e.key==key || depth >= e.depth){
            e.key=key;
            e.depth=(int8_t)std::clamp(depth, 0, 127);
            e.score=(int16_t)std::clamp(score, -32767, 32767);
            e.flag=flag;
            e.best=best;
        }
    }
};

// ======================== Board ========================
struct Board {
    std::array<Piece, 64> b{};
    Color stm = Color::White;

    int epSquare = -1;          // en passant target square index or -1
    u8 castling = 0b1111;       // 1=WK,2=WQ,4=BK,8=BQ
    int halfmoveClock = 0;      // 50-move heuristic
    u64 hash = 0;

    const Zobrist* z = nullptr;

    void clear(){
        for(auto& p : b) p = Piece{};
        stm = Color::White;
        epSquare = -1;
        castling = 0b1111;
        halfmoveClock = 0;
        hash = 0;
    }

    void reset(){
        clear();
        auto set = [&](int file, int rank, Color c, PieceType t){
            b[rank*8 + file] = Piece{t,c};
        };

        // White
        set(0,0,Color::White,PieceType::Rook);
        set(1,0,Color::White,PieceType::Knight);
        set(2,0,Color::White,PieceType::Bishop);
        set(3,0,Color::White,PieceType::Queen);
        set(4,0,Color::White,PieceType::King);
        set(5,0,Color::White,PieceType::Bishop);
        set(6,0,Color::White,PieceType::Knight);
        set(7,0,Color::White,PieceType::Rook);
        for(int f=0; f<8; f++) set(f,1,Color::White,PieceType::Pawn);

        // Black
        set(0,7,Color::Black,PieceType::Rook);
        set(1,7,Color::Black,PieceType::Knight);
        set(2,7,Color::Black,PieceType::Bishop);
        set(3,7,Color::Black,PieceType::Queen);
        set(4,7,Color::Black,PieceType::King);
        set(5,7,Color::Black,PieceType::Bishop);
        set(6,7,Color::Black,PieceType::Knight);
        set(7,7,Color::Black,PieceType::Rook);
        for(int f=0; f<8; f++) set(f,6,Color::Black,PieceType::Pawn);

        stm = Color::White;
        epSquare = -1;
        castling = 0b1111;
        halfmoveClock=0;

        recomputeHash();
    }

    Piece at(int idx) const { return b[idx]; }
    Piece& atRef(int idx) { return b[idx]; }

    void setZobrist(const Zobrist* zz){
        z = zz;
        recomputeHash();
    }

    void recomputeHash(){
        if(!z){ hash=0; return; }
        u64 h=0;
        for(int i=0;i<64;i++){
            Piece p=b[i];
            if(isNone(p)) continue;
            int c = (p.c==Color::White)?0:1;
            int pt = (int)p.t;
            h ^= z->psq[c][pt][i];
        }
        if(stm==Color::Black) h ^= z->sideToMove;
        h ^= z->castling[castling & 0xF];
        int epF = 8;
        if(epSquare>=0) epF = epSquare % 8;
        h ^= z->epFile[epF];
        hash = h;
    }

    int findKing(Color c) const {
        for(int i=0;i<64;i++){
            auto p=b[i];
            if(p.t==PieceType::King && p.c==c) return i;
        }
        return -1;
    }

    bool isSquareAttacked(int sq, Color by) const {
        int r = sq/8, f = sq%8;

        int pr = r + ((by==Color::White) ? -1 : 1);
        if(pr>=0 && pr<8){
            if(f-1>=0){
                Piece p=b[pr*8 + (f-1)];
                if(p.t==PieceType::Pawn && p.c==by) return true;
            }
            if(f+1<8){
                Piece p=b[pr*8 + (f+1)];
                if(p.t==PieceType::Pawn && p.c==by) return true;
            }
        }

        static const int kD[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
        for(auto& d: kD){
            int nf=f+d[0], nr=r+d[1];
            if(nf>=0&&nf<8&&nr>=0&&nr<8){
                Piece p=b[nr*8+nf];
                if(p.t==PieceType::Knight && p.c==by) return true;
            }
        }

        for(int df=-1; df<=1; df++){
            for(int dr=-1; dr<=1; dr++){
                if(df==0&&dr==0) continue;
                int nf=f+df, nr=r+dr;
                if(nf>=0&&nf<8&&nr>=0&&nr<8){
                    Piece p=b[nr*8+nf];
                    if(p.t==PieceType::King && p.c==by) return true;
                }
            }
        }

        auto ray = [&](int df, int dr, PieceType a, PieceType b2)->bool{
            int nf=f+df, nr=r+dr;
            while(nf>=0&&nf<8&&nr>=0&&nr<8){
                Piece p=this->b[nr*8+nf];
                if(!isNone(p)){
                    if(p.c==by && (p.t==a || p.t==b2)) return true;
                    return false;
                }
                nf+=df; nr+=dr;
            }
            return false;
        };

        if(ray(1,1,PieceType::Bishop,PieceType::Queen)) return true;
        if(ray(1,-1,PieceType::Bishop,PieceType::Queen)) return true;
        if(ray(-1,1,PieceType::Bishop,PieceType::Queen)) return true;
        if(ray(-1,-1,PieceType::Bishop,PieceType::Queen)) return true;

        if(ray(1,0,PieceType::Rook,PieceType::Queen)) return true;
        if(ray(-1,0,PieceType::Rook,PieceType::Queen)) return true;
        if(ray(0,1,PieceType::Rook,PieceType::Queen)) return true;
        if(ray(0,-1,PieceType::Rook,PieceType::Queen)) return true;

        return false;
    }

    bool inCheck(Color c) const {
        int k = findKing(c);
        if(k<0) return false;
        return isSquareAttacked(k, other(c));
    }

    void genPseudoMoves(std::vector<Move>& out) const {
        out.clear();
        Color us = stm;

        auto push = [&](int from, int to, bool cap=false, bool ep=false, bool castle=false, PieceType promo=PieceType::None){
            Move m;
            m.from=(u8)from; m.to=(u8)to;
            m.isCapture=cap; m.isEnPassant=ep; m.isCastle=castle; m.promo=promo;
            out.push_back(m);
        };

        for(int i=0;i<64;i++){
            Piece p=b[i];
            if(isNone(p) || p.c!=us) continue;
            int r=i/8, f=i%8;

            if(p.t==PieceType::Pawn){
                int dir = (us==Color::White) ? 1 : -1;
                int startRank = (us==Color::White) ? 1 : 6;
                int promoRank = (us==Color::White) ? 7 : 0;

                int nr = r + dir;
                if(nr>=0 && nr<8){
                    int one = nr*8 + f;
                    if(isNone(b[one])){
                        if(nr==promoRank){
                            push(i, one, false, false, false, PieceType::Queen);
                            push(i, one, false, false, false, PieceType::Rook);
                            push(i, one, false, false, false, PieceType::Bishop);
                            push(i, one, false, false, false, PieceType::Knight);
                        } else {
                            push(i, one);
                            if(r==startRank){
                                int twoR = r + 2*dir;
                                int two = twoR*8 + f;
                                if(twoR>=0 && twoR<8 && isNone(b[two])) push(i, two);
                            }
                        }
                    }
                }

                for(int df : {-1,1}){
                    int nf=f+df, tr=r+dir;
                    if(nf<0||nf>=8||tr<0||tr>=8) continue;
                    int to=tr*8+nf;

                    if(!isNone(b[to]) && b[to].c!=us){
                        if(tr==promoRank){
                            push(i,to,true,false,false,PieceType::Queen);
                            push(i,to,true,false,false,PieceType::Rook);
                            push(i,to,true,false,false,PieceType::Bishop);
                            push(i,to,true,false,false,PieceType::Knight);
                        } else push(i,to,true);
                    }

                    if(epSquare==to){
                        int adj = r*8 + nf;
                        if(!isNone(b[adj]) && b[adj].t==PieceType::Pawn && b[adj].c!=us){
                            push(i,to,true,true,false);
                        }
                    }
                }

            } else if(p.t==PieceType::Knight){
                static const int d[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
                for(auto& dd: d){
                    int nf=f+dd[0], nr=r+dd[1];
                    if(nf<0||nf>=8||nr<0||nr>=8) continue;
                    int to=nr*8+nf;
                    if(isNone(b[to])) push(i,to);
                    else if(b[to].c!=us) push(i,to,true);
                }

            } else if(p.t==PieceType::Bishop || p.t==PieceType::Rook || p.t==PieceType::Queen){
                auto slide=[&](int df,int dr){
                    int nf=f+df, nr=r+dr;
                    while(nf>=0&&nf<8&&nr>=0&&nr<8){
                        int to=nr*8+nf;
                        if(isNone(b[to])) push(i,to);
                        else {
                            if(b[to].c!=us) push(i,to,true);
                            break;
                        }
                        nf+=df; nr+=dr;
                    }
                };

                if(p.t==PieceType::Bishop || p.t==PieceType::Queen){
                    slide(1,1); slide(1,-1); slide(-1,1); slide(-1,-1);
                }
                if(p.t==PieceType::Rook || p.t==PieceType::Queen){
                    slide(1,0); slide(-1,0); slide(0,1); slide(0,-1);
                }

            } else if(p.t==PieceType::King){
                for(int df=-1; df<=1; df++){
                    for(int dr=-1; dr<=1; dr++){
                        if(df==0&&dr==0) continue;
                        int nf=f+df, nr=r+dr;
                        if(nf<0||nf>=8||nr<0||nr>=8) continue;
                        int to=nr*8+nf;
                        if(isNone(b[to])) push(i,to);
                        else if(b[to].c!=us) push(i,to,true);
                    }
                }

                if(us==Color::White && i==4){
                    if((castling & 0b0001) && isNone(b[5]) && isNone(b[6]) &&
                       b[7].t==PieceType::Rook && b[7].c==Color::White){
                        if(!inCheck(Color::White) &&
                           !isSquareAttacked(5, Color::Black) &&
                           !isSquareAttacked(6, Color::Black))
                            push(4,6,false,false,true);
                    }
                    if((castling & 0b0010) && isNone(b[3]) && isNone(b[2]) && isNone(b[1]) &&
                       b[0].t==PieceType::Rook && b[0].c==Color::White){
                        if(!inCheck(Color::White) &&
                           !isSquareAttacked(3, Color::Black) &&
                           !isSquareAttacked(2, Color::Black))
                            push(4,2,false,false,true);
                    }
                }
                if(us==Color::Black && i==60){
                    if((castling & 0b0100) && isNone(b[61]) && isNone(b[62]) &&
                       b[63].t==PieceType::Rook && b[63].c==Color::Black){
                        if(!inCheck(Color::Black) &&
                           !isSquareAttacked(61, Color::White) &&
                           !isSquareAttacked(62, Color::White))
                            push(60,62,false,false,true);
                    }
                    if((castling & 0b1000) && isNone(b[59]) && isNone(b[58]) && isNone(b[57]) &&
                       b[56].t==PieceType::Rook && b[56].c==Color::Black){
                        if(!inCheck(Color::Black) &&
                           !isSquareAttacked(59, Color::White) &&
                           !isSquareAttacked(58, Color::White))
                            push(60,58,false,false,true);
                    }
                }
            }
        }
    }

    bool makeMove(const Move& m, Undo& u){
        u.m = m;
        u.epSquare = epSquare;
        u.castling = castling;
        u.halfmoveClock = halfmoveClock;
        u.hash = hash;
        u.captured = Piece{};

        Piece moving = b[m.from];
        if(isNone(moving)) return false;

        bool resetHalf = (moving.t==PieceType::Pawn) || m.isCapture || m.isEnPassant;
        halfmoveClock = resetHalf ? 0 : (halfmoveClock + 1);

        if(z){
            int oldEpF = (epSquare>=0) ? (epSquare%8) : 8;
            hash ^= z->epFile[oldEpF];
            hash ^= z->castling[castling & 0xF];
            if(stm==Color::Black) hash ^= z->sideToMove;
        }

        epSquare = -1;

        if(m.isEnPassant){
            int dir = (moving.c==Color::White) ? -8 : 8;
            int capSq = int(m.to) + dir;
            u.captured = b[capSq];
            if(z && !isNone(u.captured)){
                int cc = (u.captured.c==Color::White)?0:1;
                hash ^= z->psq[cc][(int)u.captured.t][capSq];
            }
            b[capSq] = Piece{};
        } else if(m.isCapture){
            u.captured = b[m.to];
            if(z && !isNone(u.captured)){
                int cc = (u.captured.c==Color::White)?0:1;
                hash ^= z->psq[cc][(int)u.captured.t][(int)m.to];
            }
        }

        if(z){
            int mc = (moving.c==Color::White)?0:1;
            hash ^= z->psq[mc][(int)moving.t][(int)m.from];
        }

        b[m.to] = b[m.from];
        b[m.from] = Piece{};

        if(z){
            int mc = (moving.c==Color::White)?0:1;
            hash ^= z->psq[mc][(int)moving.t][(int)m.to];
        }

        if(m.promo != PieceType::None){
            if(z){
                int mc = (moving.c==Color::White)?0:1;
                hash ^= z->psq[mc][(int)PieceType::Pawn][(int)m.to];
                hash ^= z->psq[mc][(int)m.promo][(int)m.to];
            }
            b[m.to].t = m.promo;
        }

        if(m.isCastle){
            if(moving.c==Color::White){
                if(m.to==6){
                    Piece rook=b[7];
                    if(z){
                        int rc=0;
                        hash ^= z->psq[rc][(int)rook.t][7];
                        hash ^= z->psq[rc][(int)rook.t][5];
                    }
                    b[5]=b[7]; b[7]=Piece{};
                } else if(m.to==2){
                    Piece rook=b[0];
                    if(z){
                        int rc=0;
                        hash ^= z->psq[rc][(int)rook.t][0];
                        hash ^= z->psq[rc][(int)rook.t][3];
                    }
                    b[3]=b[0]; b[0]=Piece{};
                }
            } else {
                if(m.to==62){
                    Piece rook=b[63];
                    if(z){
                        int rc=1;
                        hash ^= z->psq[rc][(int)rook.t][63];
                        hash ^= z->psq[rc][(int)rook.t][61];
                    }
                    b[61]=b[63]; b[63]=Piece{};
                } else if(m.to==58){
                    Piece rook=b[56];
                    if(z){
                        int rc=1;
                        hash ^= z->psq[rc][(int)rook.t][56];
                        hash ^= z->psq[rc][(int)rook.t][59];
                    }
                    b[59]=b[56]; b[56]=Piece{};
                }
            }
        }

        auto clearIfTouches = [&](int sq, u8 mask){
            if(int(m.from)==sq || int(m.to)==sq) castling &= ~mask;
        };
        clearIfTouches(4,  0b0011);
        clearIfTouches(0,  0b0010);
        clearIfTouches(7,  0b0001);
        clearIfTouches(60, 0b1100);
        clearIfTouches(56, 0b1000);
        clearIfTouches(63, 0b0100);

        if(moving.t==PieceType::Pawn){
            int fr = int(m.from)/8;
            int tr = int(m.to)/8;
            if(std::abs(tr - fr) == 2){
                epSquare = (int(m.from) + int(m.to))/2;
            }
        }

        stm = other(stm);

        if(inCheck(other(stm))){
            undoMove(u);
            return false;
        }

        if(z){
            int newEpF = (epSquare>=0) ? (epSquare%8) : 8;
            hash ^= z->epFile[newEpF];
            hash ^= z->castling[castling & 0xF];
            if(stm==Color::Black) hash ^= z->sideToMove;
        }

        return true;
    }

    void undoMove(const Undo& u){
        const Move& m = u.m;

        stm = other(stm);

        epSquare = u.epSquare;
        castling = u.castling;
        halfmoveClock = u.halfmoveClock;
        hash = u.hash;

        Piece moved = b[m.to];

        if(m.isCastle){
            if(moved.c==Color::White){
                if(m.to==6){ b[7]=b[5]; b[5]=Piece{}; }
                else if(m.to==2){ b[0]=b[3]; b[3]=Piece{}; }
            } else {
                if(m.to==62){ b[63]=b[61]; b[61]=Piece{}; }
                else if(m.to==58){ b[56]=b[59]; b[59]=Piece{}; }
            }
        }

        b[m.from] = b[m.to];
        b[m.to] = Piece{};

        if(m.promo != PieceType::None){
            b[m.from].t = PieceType::Pawn;
        }

        if(m.isEnPassant){
            int dir = (b[m.from].c==Color::White) ? -8 : 8;
            int capSq = int(m.to) + dir;
            b[capSq] = u.captured;
        } else if(m.isCapture){
            b[m.to] = u.captured;
        }
    }

    void genLegalMoves(std::vector<Move>& legal){
        std::vector<Move> pseudo;
        genPseudoMoves(pseudo);
        legal.clear();
        legal.reserve(pseudo.size());

        for(const auto& m : pseudo){
            Undo u{};
            if(makeMove(m,u)){
                legal.push_back(m);
                undoMove(u);
            }
        }
    }

    void genLegalMovesFrom(int from, std::vector<Move>& out){
        std::vector<Move> legal;
        genLegalMoves(legal);
        out.clear();
        for(const auto& m: legal) if(m.from==from) out.push_back(m);
    }

    bool insufficientMaterial() const {
        int wMinor=0,bMinor=0;
        int wB=0,wN=0,bB=0,bN=0;
        int wOther=0,bOther=0;
        for(auto& p: b){
            if(isNone(p) || p.t==PieceType::King) continue;
            if(p.t==PieceType::Pawn || p.t==PieceType::Rook || p.t==PieceType::Queen){
                if(p.c==Color::White) wOther++;
                else bOther++;
            } else {
                if(p.c==Color::White){ wMinor++; if(p.t==PieceType::Bishop) wB++; if(p.t==PieceType::Knight) wN++; }
                else { bMinor++; if(p.t==PieceType::Bishop) bB++; if(p.t==PieceType::Knight) bN++; }
            }
        }
        if(wOther>0 || bOther>0) return false;
        if(wMinor==0 && bMinor==0) return true;
        if(wMinor==1 && bMinor==0 && (wB==1 || wN==1)) return true;
        if(bMinor==1 && wMinor==0 && (bB==1 || bN==1)) return true;
        if(wMinor==1 && bMinor==1 && wB==1 && bB==1) return true;
        return false;
    }
};

// ======================== Evaluation (PST + extras) ========================
static int mirrorIndex(int idx){
    int f = idx%8, r=idx/8;
    int mr = 7-r;
    return mr*8 + f;
}

static const int PST_PAWN[64]={
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 55, 55, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int PST_KNIGHT[64]={
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};
static const int PST_BISHOP[64]={
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};
static const int PST_ROOK[64]={
     0,  0,  5, 10, 10,  5,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int PST_QUEEN[64]={
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};
static const int PST_KING_MG[64]={
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};
static const int PST_KING_EG[64]={
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50
};

static int pstScore(PieceType t, int idxWhitePerspective, bool endgameKing){
    switch(t){
        case PieceType::Pawn: return PST_PAWN[idxWhitePerspective];
        case PieceType::Knight: return PST_KNIGHT[idxWhitePerspective];
        case PieceType::Bishop: return PST_BISHOP[idxWhitePerspective];
        case PieceType::Rook: return PST_ROOK[idxWhitePerspective];
        case PieceType::Queen: return PST_QUEEN[idxWhitePerspective];
        case PieceType::King: return endgameKing ? PST_KING_EG[idxWhitePerspective] : PST_KING_MG[idxWhitePerspective];
        default: return 0;
    }
}

static int evaluate(const Board& bd){
    int material=0;
    int pst=0;

    int phase=0;
    for(int i=0;i<64;i++){
        Piece p=bd.b[i];
        if(isNone(p) || p.t==PieceType::King || p.t==PieceType::Pawn) continue;
        if(p.t==PieceType::Knight || p.t==PieceType::Bishop) phase += 1;
        else if(p.t==PieceType::Rook) phase += 2;
        else if(p.t==PieceType::Queen) phase += 4;
    }
    phase = std::clamp(phase, 0, 24);
    bool endgameKing = (phase <= 8);

    int whiteBishops=0, blackBishops=0;
    int wpFile[8]{}, bpFile[8]{};

    for(int i=0;i<64;i++){
        Piece p=bd.b[i];
        if(isNone(p)) continue;
        int base = pieceValue(p.t);
        if(p.c==Color::White) material += base;
        else material -= base;

        int idxW = (p.c==Color::White) ? i : mirrorIndex(i);
        int ps = pstScore(p.t, idxW, endgameKing);
        if(p.c==Color::White) pst += ps;
        else pst -= ps;

        if(p.t==PieceType::Bishop){
            if(p.c==Color::White) whiteBishops++;
            else blackBishops++;
        }
        if(p.t==PieceType::Pawn){
            int f=i%8;
            if(p.c==Color::White) wpFile[f]++;
            else bpFile[f]++;
        }
    }

    int bishopPair = 0;
    if(whiteBishops>=2) bishopPair += 30;
    if(blackBishops>=2) bishopPair -= 30;

    int pawnStruct=0;
    for(int f=0;f<8;f++){
        if(wpFile[f]>=2) pawnStruct -= 12*(wpFile[f]-1);
        if(bpFile[f]>=2) pawnStruct += 12*(bpFile[f]-1);

        if(wpFile[f]>0){
            bool left = (f>0 && wpFile[f-1]>0);
            bool right= (f<7 && wpFile[f+1]>0);
            if(!left && !right) pawnStruct -= 10;
        }
        if(bpFile[f]>0){
            bool left = (f>0 && bpFile[f-1]>0);
            bool right= (f<7 && bpFile[f+1]>0);
            if(!left && !right) pawnStruct += 10;
        }
    }

    int mobility=0;
    {
        Board t=bd;
        t.stm=Color::White;
        std::vector<Move> w; t.genPseudoMoves(w);
        t.stm=Color::Black;
        std::vector<Move> b; t.genPseudoMoves(b);
        mobility = (int(w.size()) - int(b.size())) * 2;
    }

    int kingSafety=0;
    if(!endgameKing){
        int wK = bd.findKing(Color::White);
        int bK = bd.findKing(Color::Black);

        auto kingCentrePenalty = [&](int kIdx, Color c)->int{
            if(kIdx<0) return 0;
            int f=kIdx%8, r=kIdx/8;
            int df = std::abs(f-4);
            int pen = 0;
            if(df<=1 && (r==0 || r==7)) pen += 10;
            if(df<=1 && (r==1 || r==6)) pen += 20;
            if(df<=1 && (r==2 || r==5)) pen += 35;
            return pen;
        };

        kingSafety -= kingCentrePenalty(wK, Color::White);
        kingSafety += kingCentrePenalty(bK, Color::Black);

        bool wCanCastle = (bd.castling & 0b0011);
        bool bCanCastle = (bd.castling & 0b1100);
        if(!wCanCastle) kingSafety -= 10;
        if(!bCanCastle) kingSafety += 10;
    }

    int scoreWhite = material + pst + bishopPair + pawnStruct + mobility + kingSafety;
    return (bd.stm==Color::White) ? scoreWhite : -scoreWhite;
}

// ======================== Search (ID + TT + QS + Ordering) ========================
struct SearchStats {
    u64 nodes=0;
    u64 qnodes=0;
    int depthReached=0;
    int bestScore=0;
    int timeMs=0;
};

struct SearchContext {
    TranspositionTable tt;
    SearchStats stats;
    std::chrono::steady_clock::time_point start;
    int timeLimitMs=1000;
    bool stop=false;

    Move killer[128][2]{};
    int history[2][64][64]{};
    std::vector<u64> repetition;
};

static bool sameMove(const Move& a, const Move& b){
    return a.from==b.from && a.to==b.to && a.promo==b.promo && a.isCastle==b.isCastle && a.isEnPassant==b.isEnPassant;
}

static int mvvLvaScore(const Board& bd, const Move& m){
    Piece a = bd.at(m.from);
    int attacker = pieceValue(a.t);
    int victim = 0;
    if(m.isEnPassant){
        victim = pieceValue(PieceType::Pawn);
    } else if(m.isCapture){
        Piece v = bd.at(m.to);
        victim = pieceValue(v.t);
    }
    return victim*10 - attacker;
}

static int scoreMove(const Board& bd, SearchContext& ctx, const Move& m, const Move& ttMove, int ply){
    if(ttMove.from==m.from && ttMove.to==m.to && ttMove.promo==m.promo) return 1000000;

    if(m.isCapture || m.isEnPassant){
        return 100000 + mvvLvaScore(bd, m);
    }

    if(ply<128){
        if(sameMove(m, ctx.killer[ply][0])) return 90000;
        if(sameMove(m, ctx.killer[ply][1])) return 80000;
    }

    int side = (bd.stm==Color::White)?0:1;
    return ctx.history[side][m.from][m.to];
}

static inline bool timeUp(SearchContext& ctx){
    if(ctx.stop) return true;
    auto now = std::chrono::steady_clock::now();
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start).count();
    if(ms >= ctx.timeLimitMs){
        ctx.stop=true;
        return true;
    }
    return false;
}

static const int INF = 100000000;
static const int MATE = 1000000;

static int quiescence(Board& bd, SearchContext& ctx, int alpha, int beta){
    if(timeUp(ctx)) return 0;
    ctx.stats.qnodes++;

    int stand = evaluate(bd);
    if(stand >= beta) return beta;
    if(stand > alpha) alpha = stand;

    std::vector<Move> pseudo;
    bd.genPseudoMoves(pseudo);

    std::vector<Move> moves;
    moves.reserve(pseudo.size());
    for(const auto& m: pseudo){
        if(m.isCapture || m.isEnPassant || m.promo!=PieceType::None){
            Undo u{};
            if(bd.makeMove(m,u)){
                moves.push_back(m);
                bd.undoMove(u);
            }
        }
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return mvvLvaScore(bd,a) > mvvLvaScore(bd,b);
    });

    for(const auto& m: moves){
        Undo u{};
        if(!bd.makeMove(m,u)) continue;
        int score = -quiescence(bd, ctx, -beta, -alpha);
        bd.undoMove(u);

        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(Board& bd, SearchContext& ctx, int depth, int alpha, int beta, int ply){
    if(timeUp(ctx)) return 0;
    ctx.stats.nodes++;

    if(bd.insufficientMaterial()) return 0;
    if(bd.halfmoveClock >= 100) return 0;

    int repCount=0;
    for(u64 h : ctx.repetition){
        if(h==bd.hash) repCount++;
    }
    if(repCount>=2) return 0;

    Move ttMove{};
    if(auto* e = ctx.tt.probe(bd.hash)){
        if(e->key==bd.hash){
            ttMove = e->best;
            if(e->depth >= depth){
                int s = e->score;
                if(e->flag==TTFlag::Exact) return s;
                if(e->flag==TTFlag::Lower) alpha = std::max(alpha, s);
                else if(e->flag==TTFlag::Upper) beta = std::min(beta, s);
                if(alpha >= beta) return s;
            }
        }
    }

    std::vector<Move> moves;
    bd.genLegalMoves(moves);

    if(depth==0){
        return quiescence(bd, ctx, alpha, beta);
    }

    if(moves.empty()){
        if(bd.inCheck(bd.stm)) return -MATE + ply;
        return 0;
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return scoreMove(bd, ctx, a, ttMove, ply) > scoreMove(bd, ctx, b, ttMove, ply);
    });

    int best = -INF;
    Move bestM{};

    int originalAlpha = alpha;

    for(size_t i=0;i<moves.size();i++){
        const Move& m = moves[i];

        Undo u{};
        if(!bd.makeMove(m,u)) continue;

        ctx.repetition.push_back(bd.hash);

        int newDepth = depth - 1;
        int score=0;

        bool isQuiet = !(m.isCapture || m.isEnPassant) && (m.promo==PieceType::None);
        if(newDepth >= 3 && i >= 4 && isQuiet && !bd.inCheck(bd.stm)){
            score = -negamax(bd, ctx, newDepth-1, -alpha-1, -alpha, ply+1);
            if(score > alpha){
                score = -negamax(bd, ctx, newDepth, -beta, -alpha, ply+1);
            }
        } else {
            score = -negamax(bd, ctx, newDepth, -beta, -alpha, ply+1);
        }

        ctx.repetition.pop_back();
        bd.undoMove(u);

        if(ctx.stop) return 0;

        if(score > best){
            best = score;
            bestM = m;
        }

        alpha = std::max(alpha, score);
        if(alpha >= beta){
            if(isQuiet && ply<128){
                if(!sameMove(ctx.killer[ply][0], m)){
                    ctx.killer[ply][1] = ctx.killer[ply][0];
                    ctx.killer[ply][0] = m;
                }
                int side = (bd.stm==Color::White)?0:1;
                ctx.history[side][m.from][m.to] = std::min(90000, ctx.history[side][m.from][m.to] + depth*depth*8);
            }
            break;
        }
    }

    TTFlag flag = TTFlag::Exact;
    if(best <= originalAlpha) flag = TTFlag::Upper;
    else if(best >= beta) flag = TTFlag::Lower;
    ctx.tt.store(bd.hash, depth, best, flag, bestM);

    return best;
}

static Move searchBestMove(Board& bd, SearchContext& ctx, int maxDepth, int timeLimitMs){
    ctx.stats = {};
    ctx.start = std::chrono::steady_clock::now();
    ctx.timeLimitMs = timeLimitMs;
    ctx.stop = false;

    ctx.repetition.clear();
    ctx.repetition.push_back(bd.hash);

    std::vector<Move> rootMoves;
    bd.genLegalMoves(rootMoves);
    if(rootMoves.empty()) return Move{};

    Move bestMove = rootMoves[0];
    int bestScore = -INF;

    for(int d=1; d<=maxDepth; d++){
        if(timeUp(ctx)) break;

        int alpha = -INF;
        int beta  = INF;
        if(d >= 3){
            alpha = bestScore - 50;
            beta  = bestScore + 50;
        }

        Move ttMove{};
        if(auto* e = ctx.tt.probe(bd.hash)){
            if(e->key==bd.hash) ttMove = e->best;
        }

        std::sort(rootMoves.begin(), rootMoves.end(), [&](const Move& a, const Move& b){
            return scoreMove(bd, ctx, a, ttMove, 0) > scoreMove(bd, ctx, b, ttMove, 0);
        });

        int localBest=-INF;
        Move localMove = rootMoves[0];

        for(const auto& m : rootMoves){
            if(timeUp(ctx)) break;
            Undo u{};
            if(!bd.makeMove(m,u)) continue;

            ctx.repetition.push_back(bd.hash);
            int score = -negamax(bd, ctx, d-1, -beta, -alpha, 1);
            ctx.repetition.pop_back();

            bd.undoMove(u);

            if(ctx.stop) break;

            if(score > localBest){
                localBest = score;
                localMove = m;
            }

            alpha = std::max(alpha, score);

            if(alpha >= beta){
                alpha = -INF;
                beta = INF;
                Undo u2{};
                if(bd.makeMove(m,u2)){
                    ctx.repetition.push_back(bd.hash);
                    int score2 = -negamax(bd, ctx, d-1, -INF, INF, 1);
                    ctx.repetition.pop_back();
                    bd.undoMove(u2);
                    if(!ctx.stop && score2 > localBest){
                        localBest = score2;
                        localMove = m;
                    }
                }
                break;
            }
        }

        if(!ctx.stop){
            bestScore = localBest;
            bestMove = localMove;
            ctx.stats.depthReached = d;
            ctx.stats.bestScore = bestScore;
        }
    }

    auto end = std::chrono::steady_clock::now();
    ctx.stats.timeMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - ctx.start).count();
    return bestMove;
}

// ======================== Assets ========================
struct PieceAtlas {
    std::map<std::string, sf::Texture> tex;

    bool loadAll(const std::string& dir){
        const std::vector<std::string> colors = {"white_","black_"};
        const std::vector<std::string> names  = {"king","queen","rook","bishop","knight","pawn"};

        for(const auto& c : colors){
            for(const auto& n : names){
                std::string key = c+n;
                sf::Texture t;
                if(!t.loadFromFile(dir + "/" + key + ".png")) return false;
                t.setSmooth(false);
                tex.emplace(key, std::move(t));
            }
        }
        return true;
    }

    const sf::Texture* get(const Piece& p) const {
        auto k = pieceKey(p);
        auto it = tex.find(k);
        if(it==tex.end()) return nullptr;
        return &it->second;
    }
};

// ======================== App modes ========================
enum class GameMode { Menu, PvP, PvAI, AIvAI };
static std::string modeStr(GameMode m){
    switch(m){
        case GameMode::PvP: return "PvP";
        case GameMode::PvAI: return "PvAI";
        case GameMode::AIvAI: return "AIvAI";
        default: return "Menu";
    }
}

// ======================== Main ========================
int main(){
    constexpr unsigned windowW=1240, windowH=880;
    sf::RenderWindow window(sf::VideoMode(windowW, windowH), "Chess Engine (SFML 2.6) - NEA Build");
    window.setFramerateLimit(60);

    const float tile = 96.f;
    const sf::Vector2f boardOrigin(40.f, 40.f);

    // SFML2: don't use FloatRect.position/size – use explicit vectors.
    const sf::Vector2f panelPos(boardOrigin.x + 8.f*tile + 30.f, boardOrigin.y);
    const sf::Vector2f panelSize(420.f, 8.f*tile);

    // Font: include Linux candidates (and keep your mac ones harmless)
    sf::Font font;
    bool hasFont=false;
    {
        std::vector<std::string> candidates = {
          "assets/fonts/Inter-Regular.ttf",
          // Fedora / Linux (DejaVu)
          "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
          "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
          // macOS (harmless on Linux)
          "/System/Library/Fonts/Supplemental/Arial.ttf",
          "/System/Library/Fonts/Supplemental/Verdana.ttf",
          "/System/Library/Fonts/Supplemental/Trebuchet MS.ttf"        
        };
        for(const auto& p : candidates){
            if(std::filesystem::exists(p)){
                hasFont = font.loadFromFile(p);
                if(hasFont) break;
            }
        }
    }

    PieceAtlas atlas;
    bool hasIcons = atlas.loadAll("assets/pieces_png");

    Zobrist zob;
    Board board;
    board.setZobrist(&zob);
    board.reset();

    SearchContext search;
    search.tt.resizeMB(64);

    std::vector<Undo> undoStack;
    std::vector<std::string> moveListUCI;

    auto pushMove = [&](const Move& m)->bool{
        Undo u{};
        if(board.makeMove(m, u)){
            undoStack.push_back(u);
            moveListUCI.push_back(moveToUCI(m));
            return true;
        }
        return false;
    };
    auto popUndo = [&](){
        if(undoStack.empty()) return;
        Undo u = undoStack.back();
        undoStack.pop_back();
        board.undoMove(u);
        if(!moveListUCI.empty()) moveListUCI.pop_back();
    };

    GameMode mode = GameMode::Menu;
    GameMode pending = GameMode::PvP;

    std::string status = hasIcons ? "Ready." : "Missing icons: assets/pieces_png/*.png";

    std::optional<int> selectedSq;
    std::vector<Move> selectedMoves;
    std::optional<Move> lastMove;

    bool dragging=false;
    std::optional<int> dragFrom;
    sf::Vector2f dragPos(0,0);

    int aiMaxDepth = 8;
    int aiTimeMs = 10000;
    int aiDelayMs = 350;
    sf::Clock aiClock;

    bool flipBoard=false;

    auto isHumanSide = [&](Color c)->bool{
        if(mode==GameMode::PvP) return true;
        if(mode==GameMode::PvAI) return (c==Color::White);
        return false;
    };

    auto refreshSelection = [&](){
        selectedMoves.clear();
        if(!selectedSq) return;
        board.genLegalMovesFrom(*selectedSq, selectedMoves);
    };

    auto resetGame = [&](){
        board.reset();
        undoStack.clear();
        moveListUCI.clear();
        selectedSq.reset();
        selectedMoves.clear();
        lastMove.reset();
        dragging=false;
        dragFrom.reset();
        status = "Reset.";
    };

    auto tryMoveFromTo = [&](int from, int to)->bool{
        std::vector<Move> moves;
        board.genLegalMovesFrom(from, moves);
        auto it = std::find_if(moves.begin(), moves.end(), [&](const Move& m){
            return m.to==to;
        });
        if(it==moves.end()) return false;

        Move chosen = *it;
        if(chosen.promo!=PieceType::None && chosen.promo!=PieceType::Queen){
            auto itQ = std::find_if(moves.begin(), moves.end(), [&](const Move& m){
                return m.to==to && m.promo==PieceType::Queen;
            });
            if(itQ!=moves.end()) chosen = *itQ;
        }

        if(pushMove(chosen)){
            lastMove = chosen;
            selectedSq.reset();
            selectedMoves.clear();
            status = "Played " + sqName(indexToSq(chosen.from)) + "->" + sqName(indexToSq(chosen.to));
            return true;
        }
        return false;
    };

    SearchStats lastSearchStats{};

    while(window.isOpen()){
        sf::Event e;
        while(window.pollEvent(e)){
            if(e.type == sf::Event::Closed) window.close();

            if(e.type == sf::Event::KeyPressed){
                auto code = e.key.code;

                if(code == sf::Keyboard::Escape) window.close();

                if(mode==GameMode::Menu){
                    if(code == sf::Keyboard::Num1) pending = GameMode::PvP;
                    if(code == sf::Keyboard::Num2) pending = GameMode::PvAI;
                    if(code == sf::Keyboard::Num3) pending = GameMode::AIvAI;
                    if(code == sf::Keyboard::Enter){
                        mode = pending;
                        resetGame();
                        status = "Game started: " + modeStr(mode);
                    }
                } else {
                    if(code == sf::Keyboard::R) resetGame();
                    if(code == sf::Keyboard::U) { popUndo(); status = "Undo."; }

                    if(code == sf::Keyboard::F){
                        flipBoard = !flipBoard;
                        status = std::string("Flip: ") + (flipBoard ? "ON" : "OFF");
                    }

                    // depth
                    if(code == sf::Keyboard::Equal || code == sf::Keyboard::Add){
                        aiMaxDepth = std::min(100, aiMaxDepth+1);
                        status = "AI max depth = " + std::to_string(aiMaxDepth);
                    }
                    if(code == sf::Keyboard::Hyphen || code == sf::Keyboard::Subtract){
                        aiMaxDepth = std::max(1, aiMaxDepth-1);
                        status = "AI max depth = " + std::to_string(aiMaxDepth);
                    }

                    // time per move
                    if(code == sf::Keyboard::T){
                        aiTimeMs = std::min(5000, aiTimeMs + 100);
                        status = "AI time = " + std::to_string(aiTimeMs) + "ms";
                    }
                    if(code == sf::Keyboard::Y){
                        aiTimeMs = std::max(100, aiTimeMs - 100);
                        status = "AI time = " + std::to_string(aiTimeMs) + "ms";
                    }
                }
            }

            if(mode!=GameMode::Menu){
                if(isHumanSide(board.stm)){
                    if(e.type == sf::Event::MouseButtonPressed){
                        if(e.mouseButton.button == sf::Mouse::Left){
                            sf::Vector2f mp(float(e.mouseButton.x), float(e.mouseButton.y));
                            auto sq = pixelToSquare(mp, tile, boardOrigin, flipBoard);
                            if(!sq) continue;
                            int idx = sqToIndex(*sq);

                            selectedSq = idx;
                            refreshSelection();

                            Piece p = board.at(idx);
                            if(!isNone(p) && p.c==board.stm){
                                dragging=true;
                                dragFrom=idx;
                                dragPos=mp;
                            }
                        }
                    }

                    if(e.type == sf::Event::MouseMoved){
                        if(dragging) dragPos = sf::Vector2f(float(e.mouseMove.x), float(e.mouseMove.y));
                    }

                    if(e.type == sf::Event::MouseButtonReleased){
                        if(e.mouseButton.button == sf::Mouse::Left){
                            if(dragging && dragFrom){
                                sf::Vector2f mp(float(e.mouseButton.x), float(e.mouseButton.y));
                                auto sq = pixelToSquare(mp, tile, boardOrigin, flipBoard);
                                if(sq){
                                    int to = sqToIndex(*sq);
                                    bool ok = tryMoveFromTo(*dragFrom, to);
                                    if(!ok) status = "Illegal move.";
                                }
                            }
                            dragging=false;
                            dragFrom.reset();
                        }
                    }
                }
            }
        }

        // AI turn
        if(mode!=GameMode::Menu && !isHumanSide(board.stm)){
            bool shouldMove = true;
            if(mode==GameMode::AIvAI){
                shouldMove = (aiClock.getElapsedTime().asMilliseconds() >= aiDelayMs);
            }
            if(shouldMove){
                std::vector<Move> legal;
                board.genLegalMoves(legal);
                if(!legal.empty()){
                    Move m = searchBestMove(board, search, aiMaxDepth, aiTimeMs);
                    lastSearchStats = search.stats;

                    if(pushMove(m)){
                        lastMove = m;
                        aiClock.restart();
                        status = "AI: " + moveToUCI(m);
                    }
                }
            }
        }

        window.clear(sf::Color(15,15,18));

        // -------- Menu --------
        if(mode==GameMode::Menu){
            if(hasFont){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(30);
                t.setFillColor(sf::Color(240,240,240));
                t.setString("Choose mode:\n\n1) Player vs Player\n2) Player vs AI (you are White)\n3) Watch AI vs AI\n\nPress Enter to start");
                t.setPosition(snap(sf::Vector2f(60.f, 80.f)));
                window.draw(t);

                sf::Text s;
                s.setFont(font);
                s.setCharacterSize(22);
                s.setFillColor(sf::Color(230,230,230));
                s.setString("Selected: " + modeStr(pending));
                s.setPosition(snap(sf::Vector2f(60.f, 380.f)));
                window.draw(s);

                sf::Text a;
                a.setFont(font);
                a.setCharacterSize(18);
                a.setFillColor(sf::Color(200,200,200));
                a.setString(std::string("Icons: ") + (hasIcons ? "loaded" : "missing (assets/pieces_png)"));
                a.setPosition(snap(sf::Vector2f(60.f, 430.f)));
                window.draw(a);
            }
            window.display();
            continue;
        }

        // -------- Draw board --------
        for(int r=0;r<8;r++){
            for(int f=0;f<8;f++){
                Square s{f,r};
                int idx = sqToIndex(s);

                sf::RectangleShape rect(sf::Vector2f(tile,tile));
                rect.setPosition(snap(squareToPixel(s, tile, boardOrigin, flipBoard)));

                bool dark = ((f+r)%2)==1;
                sf::Color base = dark ? sf::Color(70,70,82) : sf::Color(210,210,220);

                if(lastMove && (idx==lastMove->from || idx==lastMove->to)) base = lighten(base, 30);
                if(selectedSq && idx==*selectedSq) base = lighten(base, 55);

                rect.setFillColor(base);
                window.draw(rect);
            }
        }

        for(const auto& m : selectedMoves){
            sf::RectangleShape hl(sf::Vector2f(tile,tile));
            hl.setPosition(snap(squareToPixel(indexToSq(m.to), tile, boardOrigin, flipBoard)));
            hl.setFillColor(sf::Color(80,180,120,90));
            window.draw(hl);
        }

        for(Color c : {Color::White, Color::Black}){
            if(board.inCheck(c)){
                int k = board.findKing(c);
                if(k>=0){
                    sf::RectangleShape red(sf::Vector2f(tile,tile));
                    red.setPosition(snap(squareToPixel(indexToSq(k), tile, boardOrigin, flipBoard)));
                    red.setFillColor(sf::Color(220,60,60,90));
                    window.draw(red);
                }
            }
        }

        if(hasFont){
            for(int f=0; f<8; f++){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(14);
                t.setFillColor(sf::Color(30,30,30));
                t.setString(std::string(1, char('a'+f)));
                float y = flipBoard ? (boardOrigin.y + 0.f*tile + 6.f) : (boardOrigin.y + 8.f*tile + 6.f);
                t.setPosition(snap(sf::Vector2f(boardOrigin.x + (flipBoard?(7-f):f)*tile + 4.f, y)));
                window.draw(t);
            }
            for(int r=0; r<8; r++){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(14);
                t.setFillColor(sf::Color(30,30,30));
                t.setString(std::to_string(r+1));
                int rr = flipBoard ? (7-r) : r;
                auto pos = squareToPixel(Square{0,rr}, tile, boardOrigin, flipBoard);
                t.setPosition(snap(sf::Vector2f(boardOrigin.x - 18.f, pos.y + 4.f)));
                window.draw(t);
            }
        }

        auto drawPiece = [&](const Piece& p, sf::Vector2f pos){
            if(!hasIcons) return;
            const sf::Texture* tex = atlas.get(p);
            if(!tex) return;
            sf::Sprite spr(*tex);
            auto sz = tex->getSize();
            spr.setScale(sf::Vector2f(tile/float(sz.x), tile/float(sz.y)));
            spr.setPosition(snap(pos));
            window.draw(spr);
        };

        for(int i=0;i<64;i++){
            if(dragging && dragFrom && i==*dragFrom) continue;
            Piece p = board.at(i);
            if(isNone(p)) continue;
            drawPiece(p, squareToPixel(indexToSq(i), tile, boardOrigin, flipBoard));
        }

        if(dragging && dragFrom){
            Piece p = board.at(*dragFrom);
            if(!isNone(p)){
                drawPiece(p, snap(sf::Vector2f(dragPos.x - tile/2.f, dragPos.y - tile/2.f)));
            }
        }

        // panel
        sf::RectangleShape panelBg(panelSize);
        panelBg.setPosition(panelPos);
        panelBg.setFillColor(sf::Color(25,25,30));
        window.draw(panelBg);

        if(hasFont){
            auto line = [&](float y, const std::string& txt, int size=16, sf::Color col=sf::Color(230,230,230)){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(size);
                t.setFillColor(col);
                t.setString(txt);
                t.setPosition(snap(sf::Vector2f(panelPos.x + 14.f, panelPos.y + y)));
                window.draw(t);
            };

            line(16.f, "Mode: " + modeStr(mode), 18);
            line(46.f, std::string("Turn: ") + (board.stm==Color::White ? "White" : "Black"));
            line(74.f, "AI: maxDepth " + std::to_string(aiMaxDepth) + " (+/-), time " + std::to_string(aiTimeMs) + "ms (T/Y)");
            line(102.f, "R reset   U undo   F flip   Esc quit", 14, sf::Color(200,200,200));

            std::vector<Move> moves;
            board.genLegalMoves(moves);
            if(moves.empty()){
                if(board.inCheck(board.stm)) line(140.f, "State: CHECKMATE", 18, sf::Color(255,180,180));
                else line(140.f, "State: STALEMATE", 18, sf::Color(255,220,180));
            } else {
                if(board.inCheck(board.stm)) line(140.f, "State: CHECK", 18, sf::Color(255,200,160));
            }
            if(board.insufficientMaterial()) line(170.f, "Note: insufficient material draw likely", 14, sf::Color(200,200,200));

            {
                std::ostringstream oss;
                oss << "Last AI search: depth " << lastSearchStats.depthReached
                    << ", score " << lastSearchStats.bestScore
                    << ", nodes " << lastSearchStats.nodes
                    << " (q " << lastSearchStats.qnodes << ")"
                    << ", " << lastSearchStats.timeMs << "ms";
                line(204.f, oss.str(), 14, sf::Color(200,200,200));
            }

            line(234.f, "Status:", 16);
            line(258.f, status, 14, sf::Color(220,220,220));

            if(selectedSq){
                line(304.f, "Selected: " + sqName(indexToSq(*selectedSq)), 16);
                line(330.f, "Legal moves: " + std::to_string((int)selectedMoves.size()), 14, sf::Color(200,200,200));
            }

            line(372.f, "Moves:", 16);
            int y=396;
            int start = std::max(0, (int)moveListUCI.size()-18);
            for(int i=start; i<(int)moveListUCI.size(); i++){
                std::string prefix = (i%2==0) ? (std::to_string(i/2 + 1) + ". ") : "   ";
                line((float)y, prefix + moveListUCI[i], 14, sf::Color(210,210,210));
                y += 18;
                if(y > (int)(panelPos.y + panelSize.y - 20.f)) break;
            }
        }

        window.display();
    }

    return 0;
}
