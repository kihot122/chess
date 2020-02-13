#include <cstdint>
#include <WinSock2.h>
#include <pthread.h>
#include <unistd.h>
#include <mutex>
#include <iostream>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <random>
#include <chrono>

std::mutex m_Message;

const uint8_t EMPTY = 0;
const uint8_t WHITE_PAWN = 1;
const uint8_t WHITE_KNIGHT = 2;
const uint8_t WHITE_BISHOP = 3;
const uint8_t WHITE_ROOK = 4;
const uint8_t WHITE_QUEEN = 5;
const uint8_t WHITE_KING = 6;
const uint8_t BLACK_PAWN = 7;
const uint8_t BLACK_KNIGHT = 8;
const uint8_t BLACK_BISHOP = 9;
const uint8_t BLACK_ROOK = 10;
const uint8_t BLACK_QUEEN = 11;
const uint8_t BLACK_KING = 12;
const uint8_t GHOST = 13;

struct s_Sockets
{
    SOCKET Game, SendChat, RecvChat;
    uint8_t Color;
    bool CMD;
    int Depth;
};

struct s_Chessboard
{
    uint8_t Board[64];
    uint8_t Flag;
    uint8_t Turn;

    s_Chessboard()
    {
        memset(Board, 0, 64);
        Flag = 0;
        Turn = 0;
    }
    s_Chessboard(s_Chessboard &other)
    {
        memcpy(Board, other.Board, 64);
        Flag = other.Flag;
        Turn = other.Turn;
    }
    void Update(uint8_t *Source)
    {
        memcpy(Board, Source, 64);
        Flag = Source[64];
        Turn = Source[65];
    }
};

void Message(int source, std::string msg)
{
    m_Message.lock();
    if (source == 0)
        std::cout << "[SYSTEM]\t" << msg << std::endl;
    if (source == 1)
        std::cout << "[SERVER]\t" << msg << std::endl;
    if (source == 2)
        std::cout << "[HUMAN]\t" << msg << std::endl;
    if (source == 3)
        std::cout << "[DEBUG]\t" << msg << std::endl;
    if (source == 4)
        std::cout << "[CHATBOT]\t" << msg << std::endl;
    m_Message.unlock();
}

int8_t DeltaX(uint8_t Pos1, uint8_t Pos2)
{
    return Pos2 % 8 - Pos1 % 8;
}

int8_t DeltaY(uint8_t Pos1, uint8_t Pos2)
{
    return Pos2 / 8 - Pos1 / 8;
}

uint8_t _DeltaX(uint8_t Pos1, uint8_t Pos2)
{
    return std::abs(DeltaX(Pos1, Pos2));
}

uint8_t _DeltaY(uint8_t Pos1, uint8_t Pos2)
{
    return std::abs(DeltaY(Pos1, Pos2));
}

bool SameColor(uint8_t Figure, bool Color)
{
    if (!Color)
    {
        if (Figure <= 6 && Figure != EMPTY)
            return true;
    }
    else
    {
        if (Figure >= 7 && Figure != GHOST)
            return true;
    }
    return false;
}

bool EmptyOrGhost(s_Chessboard *Chessboard, uint8_t Field)
{
    if (Chessboard->Board[Field] != EMPTY && Chessboard->Board[Field] != GHOST)
        return 1;
    return 0;
}

bool KingCheck(s_Chessboard *Chessboard)
{
    //Check if current king is being attacked
    if (Chessboard->Turn == 0) //checking white king
    {
        for (int i = 0; i < 64; i++)
        {
            if (Chessboard->Board[i] == BLACK_PAWN)
            {
                if (std::abs(DeltaX(i - 9, i)) == 1 && i - 9 >= 0)
                    if (Chessboard->Board[i - 9] == WHITE_KING)
                        return true;

                if (std::abs(DeltaX(i - 7, i)) == 1)
                    if (Chessboard->Board[i - 7] == WHITE_KING)
                        return true;
            }
            if (Chessboard->Board[i] == BLACK_ROOK || Chessboard->Board[i] == BLACK_QUEEN) //check each direction
            {
                for (int x = i + 1; DeltaY(x, i) == 0 && x < 64; x++)
                {
                    if (Chessboard->Board[x] == WHITE_KING)
                        return 1;                    //hit king
                    if (EmptyOrGhost(Chessboard, x)) //hit something but not king
                        break;
                }
                for (int x = i - 1; DeltaY(x, i) == 0 && x >= 0; x--)
                {
                    if (Chessboard->Board[x] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, x))
                        break;
                }
                for (int y = i + 8; y < 64; y += 8)
                {
                    if (Chessboard->Board[y] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, y))
                        break;
                }
                for (int y = i - 8; y >= 0; y -= 8)
                {
                    if (Chessboard->Board[y] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, y))
                        break;
                }
            }
            if (Chessboard->Board[i] == BLACK_BISHOP || Chessboard->Board[i] == BLACK_QUEEN)
            {
                //check diagonalls
                for (int n = 1; _DeltaX(i, i + n * 8 + n) == _DeltaY(i, i + n * 8 + n) && i + n * 8 + n < 64; n++)
                {
                    if (Chessboard->Board[i + n * 8 + n] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i + n * 8 + n))
                        break;
                }
                for (int n = 1; _DeltaX(i, i - n * 8 + n) == _DeltaY(i, i - n * 8 + n) && i - n * 8 + n >= 0; n++)
                {
                    if (Chessboard->Board[i - n * 8 + n] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i - n * 8 + n))
                        break;
                }
                for (int n = 1; _DeltaX(i, i - n * 8 - n) == _DeltaY(i, i - n * 8 - n) && i - n * 8 - n >= 0; n++)
                {
                    if (Chessboard->Board[i - n * 8 - n] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i - n * 8 - n))
                        break;
                }
                for (int n = 1; _DeltaX(i, i + n * 8 - n) == _DeltaY(i, i + n * 8 - n) && i + n * 8 - n < 64; n++)
                {
                    if (Chessboard->Board[i + n * 8 - n] == WHITE_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i + n * 8 - n))
                        break;
                }
            }
            if (Chessboard->Board[i] == BLACK_KNIGHT)
            {
                for (int x = -1; x <= 1; x += 2)
                {
                    for (int y = -1; y <= 1; y += 2)
                    {
                        if (_DeltaX(i + y * 8 + 2 * x, i) == std::abs(2 * x) && _DeltaY(i + y * 8 + 2 * x, i) == std::abs(y) && i + y * 8 + 2 * x >= 0 && i + y * 8 + 2 * x < 64 && Chessboard->Board[i + y * 8 + 2 * x] == WHITE_KING)
                            return 1;

                        if (_DeltaX(i + 2 * y * 8 + x, i) == std::abs(x) && _DeltaY(i + 2 * y * 8 + x, i) == std::abs(2 * y) && i + 2 * y * 8 + x >= 0 && i + 2 * y * 8 + x < 64 && Chessboard->Board[i + 2 * y * 8 + x] == WHITE_KING)
                            return 1;
                    }
                }
            }
            if (Chessboard->Board[i] == BLACK_KING)
            {
                for (int x = -1; x < 2; x++)
                {
                    for (int y = -1; y < 2; y++)
                    {
                        if (_DeltaX(i + y * 8 + x, i) == std::abs(x) && _DeltaY(i + y * 8 + x, i) == std::abs(y) && i + y * 8 + x >= 0 && i + y * 8 + x < 64 && Chessboard->Board[i + y * 8 + x] == WHITE_KING)
                            return 1;
                    }
                }
            }
        }
    }
    if (Chessboard->Turn == 1) //checking black king
    {
        for (int i = 0; i < 64; i++)
        {

            if (Chessboard->Board[i] == WHITE_PAWN)
            {
                if (std::abs(DeltaX(i + 9, i)) == 1 && i + 9 < 64)
                    if (Chessboard->Board[i + 9] == BLACK_KING)
                        return true;

                if (std::abs(DeltaX(i + 7, i)) == 1)
                    if (Chessboard->Board[i + 7] == BLACK_KING)
                        return true;
            }
            if (Chessboard->Board[i] == WHITE_ROOK || Chessboard->Board[i] == WHITE_QUEEN) //check each direction
            {
                for (int x = i + 1; DeltaY(x, i) == 0 && x < 64; x++)
                {
                    if (Chessboard->Board[x] == BLACK_KING)
                        return 1;                    //hit king
                    if (EmptyOrGhost(Chessboard, x)) //hit something but not king
                        break;
                }
                for (int x = i - 1; DeltaY(x, i) == 0 && x >= 0; x--)
                {
                    if (Chessboard->Board[x] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, x))
                        break;
                }
                for (int y = i + 8; y < 64; y += 8)
                {
                    if (Chessboard->Board[y] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, y))
                        break;
                }
                for (int y = i - 8; y >= 0; y -= 8)
                {
                    if (Chessboard->Board[y] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, y))
                        break;
                }
            }
            if (Chessboard->Board[i] == WHITE_BISHOP || Chessboard->Board[i] == WHITE_QUEEN)
            {
                //check diagonalls
                for (int n = 1; _DeltaX(i, i + n * 8 + n) == _DeltaY(i, i + n * 8 + n) && i + n * 8 + n < 64; n++)
                {
                    if (Chessboard->Board[i + n * 8 + n] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i + n * 8 + n))
                        break;
                }
                for (int n = 1; _DeltaX(i, i - n * 8 + n) == _DeltaY(i, i - n * 8 + n) && i - n * 8 + n >= 0; n++)
                {
                    if (Chessboard->Board[i - n * 8 + n] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i - n * 8 + n))
                        break;
                }
                for (int n = 1; _DeltaX(i, i - n * 8 - n) == _DeltaY(i, i - n * 8 - n) && i - n * 8 - n >= 0; n++)
                {
                    if (Chessboard->Board[i - n * 8 - n] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i - n * 8 - n))
                        break;
                }
                for (int n = 1; _DeltaX(i, i + n * 8 - n) == _DeltaY(i, i + n * 8 - n) && i + n * 8 - n < 64; n++)
                {
                    if (Chessboard->Board[i + n * 8 - n] == BLACK_KING)
                        return 1;
                    if (EmptyOrGhost(Chessboard, i + n * 8 - n))
                        break;
                }
            }
            if (Chessboard->Board[i] == WHITE_KNIGHT)
            {
                for (int x = -1; x <= 1; x += 2)
                {
                    for (int y = -1; y <= 1; y += 2)
                    {
                        if (_DeltaX(i + y * 8 + 2 * x, i) == std::abs(2 * x) && _DeltaY(i + y * 8 + 2 * x, i) == std::abs(y) && i + y * 8 + 2 * x >= 0 && i + y * 8 + 2 * x < 64 && Chessboard->Board[i + y * 8 + 2 * x] == BLACK_KING)
                            return 1;

                        if (_DeltaX(i + 2 * y * 8 + x, i) == std::abs(x) && _DeltaY(i + 2 * y * 8 + x, i) == std::abs(2 * y) && i + 2 * y * 8 + x >= 0 && i + 2 * y * 8 + x < 64 && Chessboard->Board[i + 2 * y * 8 + x] == BLACK_KING)
                            return 1;
                    }
                }
            }
            if (Chessboard->Board[i] == WHITE_KING)
            {
                for (int x = -1; x < 2; x++)
                {
                    for (int y = -1; y < 2; y++)
                    {
                        if (_DeltaX(i + y * 8 + x, i) == std::abs(x) && _DeltaY(i + y * 8 + x, i) == std::abs(y) && i + y * 8 + x >= 0 && i + y * 8 + x < 64 && Chessboard->Board[i + y * 8 + x] == BLACK_KING)
                            return 1;
                    }
                }
            }
        }
    }

    return false;
}

bool Decoy(s_Chessboard *&Chessboard, char *Move, s_Chessboard *&Temp)
{
    Temp = new s_Chessboard(*Chessboard);

    int8_t exclude = -1;

    if (Chessboard->Board[Move[0]] == EMPTY)
        return 1; //moving empty field
    if (Chessboard->Turn == 0 && Chessboard->Board[Move[0]] >= 7)
        return 1; //moving wrong color
    if (Chessboard->Turn == 1 && Chessboard->Board[Move[0]] <= 6)
        return 1; //moving wrong color
    if (Chessboard->Board[Move[0]] == GHOST)
        return 1; //moving a ghost
    if (Move[0] == Move[1])
        return 1; //moving to same position
    if (Move[0] < 0 || Move[0] >= 64 || Move[1] < 0 || Move[1] >= 64)
        return 1; //out of boundaries

    if (Move[0] == 4 && Move[1] == 0 && //long white castling
        Chessboard->Board[4] == WHITE_KING && Chessboard->Board[0] == WHITE_ROOK)
    {
        if (Chessboard->Flag & 0b10000000) //flags prohibit
            return 1;
        for (int i = 1; i < 4; i++) //obstacles
            if (EmptyOrGhost(Chessboard, i))
                return 1;

        s_Chessboard *CK = new s_Chessboard(*Chessboard);
        std::swap(CK->Board[4], CK->Board[3]);
        if (KingCheck(CK))
        {
            delete CK;
            return 1;
        }
        delete CK;

        std::swap(Chessboard->Board[4], Chessboard->Board[2]);
        std::swap(Chessboard->Board[0], Chessboard->Board[3]);
        Chessboard->Flag |= 0b11000000;
    }

    else if (Move[0] == 4 && Move[1] == 7 && //short white castling
             Chessboard->Board[4] == WHITE_KING && Chessboard->Board[7] == WHITE_ROOK)
    {
        if (Chessboard->Flag & 0b01000000) //flags prohibit
            return 1;
        for (int i = 5; i < 7; i++) //obstacles
            if (EmptyOrGhost(Chessboard, i))
                return 1;

        s_Chessboard *CK = new s_Chessboard(*Chessboard);
        std::swap(CK->Board[4], CK->Board[5]);
        if (KingCheck(CK))
        {
            delete CK;
            return 1;
        }
        delete CK;

        std::swap(Chessboard->Board[4], Chessboard->Board[6]);
        std::swap(Chessboard->Board[7], Chessboard->Board[5]);
        Chessboard->Flag |= 0b11000000;
    }

    else if (Move[0] == 60 && Move[1] == 56 && //long black castling
             Chessboard->Board[60] == BLACK_KING && Chessboard->Board[56] == BLACK_ROOK)
    {
        if (Chessboard->Flag & 0b00100000) //flags prohibit
            return 1;
        for (int i = 57; i < 60; i++) //obstacles
            if (EmptyOrGhost(Chessboard, i))
                return 1;

        s_Chessboard *CK = new s_Chessboard(*Chessboard);
        std::swap(CK->Board[60], CK->Board[59]);
        if (KingCheck(CK))
        {
            delete CK;
            return 1;
        }
        delete CK;

        std::swap(Chessboard->Board[60], Chessboard->Board[58]);
        std::swap(Chessboard->Board[56], Chessboard->Board[59]);
        Chessboard->Flag |= 0b00110000;
    }

    else if (Move[0] == 60 && Move[1] == 63 && //short black castling
             Chessboard->Board[60] == BLACK_KING && Chessboard->Board[63] == BLACK_ROOK)
    {
        if (Chessboard->Flag & 0b00010000) //flags prohibit
            return 1;
        for (int i = 61; i < 63; i++) //obstacles
            if (EmptyOrGhost(Chessboard, i))
                return 1;

        s_Chessboard *CK = new s_Chessboard(*Chessboard);
        std::swap(CK->Board[60], CK->Board[61]);
        if (KingCheck(CK))
        {
            delete CK;
            return 1;
        }
        delete CK;

        std::swap(Chessboard->Board[60], Chessboard->Board[62]);
        std::swap(Chessboard->Board[63], Chessboard->Board[61]);
        Chessboard->Flag |= 0b00110000;
    }

    else if (Chessboard->Board[Move[0]] == WHITE_PAWN)
    {
        if (DeltaY(Move[0], Move[1]) == 2) //dy == 2
        {
            if (DeltaX(Move[0], Move[1]) != 0) //tried moving horizontally
                return 1;
            if (Chessboard->Board[Move[1]] != EMPTY) //not empty
                return 1;
            if (Chessboard->Board[Move[1] - 8] != EMPTY) //neither is lower field
                return 1;
            if (Move[0] / 8 != 1) //moved before
                return 1;
            else //set en passant possible
            {
                Chessboard->Board[Move[1] - 8] = GHOST;
                exclude = Move[1] - 8;
                std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
            }
        }
        else if (DeltaY(Move[0], Move[1]) == 1) //dy == 1
        {
            if (std::abs(DeltaX(Move[0], Move[1])) == 1) //dx == 1
            {
                if (Chessboard->Board[Move[1]] == EMPTY) //empty field
                    return 1;
                if (SameColor(Chessboard->Board[Move[1]], 0)) //same color
                    return 1;
                if (Chessboard->Board[Move[1]] == GHOST) //en passant possible
                {
                    Chessboard->Board[Move[1] - 8] = EMPTY;
                    std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
                }
                else //ordinary killing
                {
                    Chessboard->Board[Move[1]] = EMPTY;
                    std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
                }
            }
            if (std::abs(DeltaX(Move[0], Move[1])) == 0) //dx == 0
            {
                if (Chessboard->Board[Move[1]] != EMPTY)
                    return 1;
                else
                    std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
            }
            if (std::abs(DeltaX(Move[0], Move[1])) > 1) //nice try lmao
                return 1;
            if (Move[1] / 8 == 7) //highest rank
                Chessboard->Board[Move[1]] = WHITE_QUEEN;
        }
        else
            return 1;
    }

    else if (Chessboard->Board[Move[0]] == BLACK_PAWN)
    {
        if (DeltaY(Move[0], Move[1]) == -2) //dy == 2
        {
            if (DeltaX(Move[0], Move[1]) != 0) //tried moving horizontally
                return 1;
            if (Chessboard->Board[Move[1]] != EMPTY) //not empty
                return 1;
            if (Chessboard->Board[Move[1] + 8] != EMPTY) //neither is upper field
                return 1;
            if (Move[0] / 8 != 6) //moved before
                return 1;
            else //set en passant possible
            {
                Chessboard->Board[Move[1] + 8] = GHOST;
                exclude = Move[1] + 8;
                std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
            }
        }
        else if (DeltaY(Move[0], Move[1]) == -1) //dy == 1
        {
            if (std::abs(DeltaX(Move[0], Move[1])) == 1) //dx == 1
            {
                if (Chessboard->Board[Move[1]] == EMPTY) //empty field
                    return 1;
                if (SameColor(Chessboard->Board[Move[1]], 1)) //same color
                    return 1;
                if (Chessboard->Board[Move[1]] == GHOST) //en passant possible
                {
                    Chessboard->Board[Move[1] + 8] = EMPTY;
                    std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
                }
                else //ordinary killing
                {
                    Chessboard->Board[Move[1]] = EMPTY;
                    std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
                }
            }
            if (std::abs(DeltaX(Move[0], Move[1])) == 0) //dx == 0
            {
                if (Chessboard->Board[Move[1]] != EMPTY)
                    return 1;
                else
                    std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
            }
            if (std::abs(DeltaX(Move[0], Move[1])) > 1) //nice try lmao
                return 1;
            if (Move[1] / 8 == 0) //highest rank
                Chessboard->Board[Move[1]] = BLACK_QUEEN;
        }
        else
            return 1;
    }

    else if (Chessboard->Board[Move[0]] == WHITE_ROOK)
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        if (std::abs(DX) > 0 && std::abs(DY) > 0)
            return 1; //moving diagonally wtf?

        //iterate in every direction looking for collisions with other pieces
        if (DX > 0)
            for (int i = 1; i < DX; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i))
                    return 1;
        if (DX < 0)
            for (int i = DX + 1; i < 0; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i))
                    return 1;
        if (DY > 0)
            for (int i = 1; i < DY; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                    return 1;
        if (DY < 0)
            for (int i = DY + 1; i < 0; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                    return 1;
        if (SameColor(Chessboard->Board[Move[1]], 0))
            return 1; //if target field is same color

        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
        //remove castling ability on that rook
        if (Move[0] == 0)
            Chessboard->Flag |= 0b10000000;
        if (Move[0] == 7)
            Chessboard->Flag |= 0b01000000;
    }

    else if (Chessboard->Board[Move[0]] == BLACK_ROOK)
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        if (std::abs(DX) > 0 && std::abs(DY) > 0)
            return 1; //moving diagonally wtf?

        //iterate in every direction looking for collisions with other pieces
        if (DX > 0)
            for (int i = 1; i < DX; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i))
                    return 1;
        if (DX < 0)
            for (int i = DX + 1; i < 0; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i))
                    return 1;
        if (DY > 0)
            for (int i = 1; i < DY; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                    return 1;
        if (DY < 0)
            for (int i = DY + 1; i < 0; i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                    return 1;
        if (SameColor(Chessboard->Board[Move[1]], 1))
            return 1; //if target field is same color

        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
        //remove castling ability on that rook
        if (Move[0] == 56)
            Chessboard->Flag |= 0b00100000;
        if (Move[0] == 63)
            Chessboard->Flag |= 0b00010000;
    }

    else if (Chessboard->Board[Move[0]] == WHITE_BISHOP)
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        if (std::abs(DX) != std::abs(DY)) //this is not diagonally
            return 1;
        //iterate diagonally for collisions
        if (DX > 0 && DY > 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8 + i))
                    return 1;

        if (DX > 0 && DY < 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] - i * 8 + i))
                    return 1;

        if (DX < 0 && DY > 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8 - i))
                    return 1;

        if (DX < 0 && DY < 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] - i * 8 - i))
                    return 1;

        if (SameColor(Chessboard->Board[Move[1]], 0))
            return 1;

        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
    }

    else if (Chessboard->Board[Move[0]] == BLACK_BISHOP)
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        if (std::abs(DX) != std::abs(DY)) //this is not diagonally
            return 1;
        //iterate diagonally for collisions
        if (DX > 0 && DY > 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8 + i))
                    return 1;

        if (DX > 0 && DY < 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] - i * 8 + i))
                    return 1;

        if (DX < 0 && DY > 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] + i * 8 - i))
                    return 1;

        if (DX < 0 && DY < 0)
            for (int i = 1; i < std::abs(DX); i++)
                if (EmptyOrGhost(Chessboard, Move[0] - i * 8 - i))
                    return 1;

        if (SameColor(Chessboard->Board[Move[1]], 1))
            return 1;

        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
    }

    else if (Chessboard->Board[Move[0]] == WHITE_KNIGHT) //defend the light of the might white knight
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        int8_t TX = std::abs(DX), TY = std::abs(DY);
        if (TX > TY)
            std::swap(TX, TY);

        if (TY != 2 || TX != 1) //not L-shaped
            return 1;

        if (SameColor(Chessboard->Board[Move[1]], 0))
            return 1;

        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
    }

    else if (Chessboard->Board[Move[0]] == BLACK_KNIGHT) //AM BATMAN
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        int8_t TX = std::abs(DX), TY = std::abs(DY);
        if (TX > TY)
            std::swap(TX, TY);

        if (TY != 2 || TX != 1) //not L-shaped
            return 1;

        if (SameColor(Chessboard->Board[Move[1]], 1))
            return 1;

        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
    }

    else if (Chessboard->Board[Move[0]] == WHITE_QUEEN)
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        if (std::abs(DX) == std::abs(DY)) //bishop rules here
        {
            //iterate diagonally for collisions
            if (DX > 0 && DY > 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8 + i))
                        return 1;

            if (DX > 0 && DY < 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] - i * 8 + i))
                        return 1;

            if (DX < 0 && DY > 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8 - i))
                        return 1;

            if (DX < 0 && DY < 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] - i * 8 - i))
                        return 1;

            if (SameColor(Chessboard->Board[Move[1]], 0))
                return 1;

            Chessboard->Board[Move[1]] = EMPTY;
            std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
        }
        else //rook rules here
        {
            if (std::abs(DX) > 0 && std::abs(DY) > 0)
                return 1; //moving diagonally wtf?

            //iterate in every direction looking for collisions with other pieces
            if (DX > 0)
                for (int i = 1; i < DX; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i))
                        return 1;
            if (DX < 0)
                for (int i = DX + 1; i < 0; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i))
                        return 1;
            if (DY > 0)
                for (int i = 1; i < DY; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                        return 1;
            if (DY < 0)
                for (int i = DY + 1; i < 0; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                        return 1;
            if (SameColor(Chessboard->Board[Move[1]], 0))
                return 1; //if target field is same color

            Chessboard->Board[Move[1]] = EMPTY;
            std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
        }
    }

    else if (Chessboard->Board[Move[0]] == BLACK_QUEEN)
    {
        int8_t DX = DeltaX(Move[0], Move[1]);
        int8_t DY = DeltaY(Move[0], Move[1]);

        if (std::abs(DX) == std::abs(DY)) //bishop rules here
        {
            //iterate diagonally for collisions
            if (DX > 0 && DY > 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8 + i))
                        return 1;

            if (DX > 0 && DY < 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] - i * 8 + i))
                        return 1;

            if (DX < 0 && DY > 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8 - i))
                        return 1;

            if (DX < 0 && DY < 0)
                for (int i = 1; i < std::abs(DX); i++)
                    if (EmptyOrGhost(Chessboard, Move[0] - i * 8 - i))
                        return 1;

            if (SameColor(Chessboard->Board[Move[1]], 1))
                return 1;

            Chessboard->Board[Move[1]] = EMPTY;
            std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
        }
        else //rook rules here
        {
            if (std::abs(DX) > 0 && std::abs(DY) > 0)
                return 1; //moving diagonally wtf?

            //iterate in every direction looking for collisions with other pieces
            if (DX > 0)
                for (int i = 1; i < DX; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i))
                        return 1;
            if (DX < 0)
                for (int i = DX + 1; i < 0; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i))
                        return 1;
            if (DY > 0)
                for (int i = 1; i < DY; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                        return 1;
            if (DY < 0)
                for (int i = DY + 1; i < 0; i++)
                    if (EmptyOrGhost(Chessboard, Move[0] + i * 8))
                        return 1;
            if (SameColor(Chessboard->Board[Move[1]], 1))
                return 1; //if target field is same color

            Chessboard->Board[Move[1]] = EMPTY;
            std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);
        }
    }

    else if (Chessboard->Board[Move[0]] == WHITE_KING)
    {
        int8_t DX = std::abs(DeltaX(Move[0], Move[1]));
        int8_t DY = std::abs(DeltaY(Move[0], Move[1]));
        if (DX > 1 || DY > 1) //reaching too far
            return 1;
        if (SameColor(Chessboard->Board[Move[1]], 0))
            return 1;
        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);

        Chessboard->Flag |= 0b11000000;
    }

    else if (Chessboard->Board[Move[0]] == BLACK_KING)
    {
        int8_t DX = std::abs(DeltaX(Move[0], Move[1]));
        int8_t DY = std::abs(DeltaY(Move[0], Move[1]));
        if (DX > 1 || DY > 1) //reaching too far
            return 1;
        if (SameColor(Chessboard->Board[Move[1]], 1))
            return 1;
        Chessboard->Board[Move[1]] = EMPTY;
        std::swap(Chessboard->Board[Move[0]], Chessboard->Board[Move[1]]);

        Chessboard->Flag |= 0b00110000;
    }

    //remove old pawn shadows
    for (int i = 0; i < 64; i++)
        if (Chessboard->Board[i] == GHOST && exclude != i)
            Chessboard->Board[i] = EMPTY;

    //KING IS BEING ATTACKED
    if (KingCheck(Chessboard))
    {
        delete Chessboard;
        Chessboard = nullptr;
        std::swap(Chessboard, Temp);
        return 1;
    }

    Chessboard->Turn = !Chessboard->Turn;
    return 0;
}

bool CheckValid(s_Chessboard *&Chessboard, char *Move)
{
    s_Chessboard *Buff;
    bool Ret = Decoy(Chessboard, Move, Buff);
    if (Buff)
        delete Buff;
    return Ret;
}

void *RecvChat(void *Data)
{
    char Buff[256]{0};
    s_Sockets *Sockets = (s_Sockets *)Data;
    while (true)
    {
        if (recv(Sockets->RecvChat, Buff, 256, 0) <= 0)
        {
            Message(0, "Server left the chat...or something");
            return nullptr;
        }
        Message(2, Buff);
        memset(Buff, 0, 256);
    }
    return nullptr;
}

int ComputeValue(s_Chessboard *Chessboard, bool Side)
{
    int val = 0;
    if (Side == 0) //computing for white
    {
        for (int i = 0; i < 64; i++)
        {
            if (Chessboard->Board[i] == WHITE_PAWN)
                val += 100;
            if (Chessboard->Board[i] == WHITE_KNIGHT)
                val += 320;
            if (Chessboard->Board[i] == WHITE_BISHOP)
                val += 330;
            if (Chessboard->Board[i] == WHITE_ROOK)
                val += 500;
            if (Chessboard->Board[i] == WHITE_QUEEN)
                val += 900;
            if (Chessboard->Board[i] == WHITE_KING)
                val += 20000;
        }
    }
    else //for black
    {
        for (int i = 0; i < 64; i++)
        {
            if (Chessboard->Board[i] == BLACK_PAWN)
                val += 100;
            if (Chessboard->Board[i] == BLACK_KNIGHT)
                val += 320;
            if (Chessboard->Board[i] == BLACK_BISHOP)
                val += 330;
            if (Chessboard->Board[i] == BLACK_ROOK)
                val += 500;
            if (Chessboard->Board[i] == BLACK_QUEEN)
                val += 900;
            if (Chessboard->Board[i] == BLACK_KING)
                val += 20000;
        }
    }
    return val;
}

int Delta(s_Chessboard *Chessboard, bool Side)
{
    int Gain = ComputeValue(Chessboard, Side);
    int Loss = ComputeValue(Chessboard, !Side);
    return Gain - Loss;
}

int Rec(s_Chessboard *Chessboard, int CurrentDepth, int MaxDepth, int Node, bool Color, char *Move, uint8_t *TableA, uint8_t *TableB, int Alpha, int Beta)
{
    if (CurrentDepth == MaxDepth) //reached bottom
        return Delta(Chessboard, Color);

    if (Node == 1) //our node (eg. first)
    {
        int max = -100000;
        char move[2];
        char t_move[2];
        int v;
        for (int i = 0; i < 64; i++)
        {
            move[0] = TableA[i];
            for (int j = 0; j < 64; j++)
            {
                move[1] = TableB[j];
                s_Chessboard *Temp = new s_Chessboard(*Chessboard);
                if (!CheckValid(Temp, move)) //this is valid move
                {
                    v = Rec(Temp, CurrentDepth + 1, MaxDepth, Node * -1, Color, Move, TableA, TableB, Alpha, Beta);
                    if (v > max)
                    {
                        max = v;
                        t_move[0] = move[0];
                        t_move[1] = move[1];
                    }
                    if (max > Alpha)
                        Alpha = max;
                    if (Alpha >= Beta)
                        break;
                }
                delete Temp;
            }
        }
        Move[0] = t_move[0];
        Move[1] = t_move[1];
        return max;
    }
    if (Node == -1) //opponent node
    {
        int min = 100000;
        char move[2];
        char t_move[2];
        int v;
        for (int i = 0; i < 64; i++)
        {
            move[0] = TableA[i];
            for (int j = 0; j < 64; j++)
            {
                move[1] = TableB[j];
                s_Chessboard *Temp = new s_Chessboard(*Chessboard);
                if (!CheckValid(Temp, move))
                {
                    v = Rec(Temp, CurrentDepth + 1, MaxDepth, Node * -1, Color, Move, TableA, TableB, Alpha, Beta);
                    if (v < min)
                    {
                        min = v;
                        t_move[0] = move[0];
                        t_move[1] = move[1];
                    }
                    if (min < Beta)
                        Beta = min;
                    if (Alpha >= Beta)
                        break;
                }
                delete Temp;
            }
        }
        Move[0] = t_move[0];
        Move[1] = t_move[1];
        return min;
    }

    return 0;
}

void DeepSearch(s_Chessboard *Chessboard, char *Move, int MaxDepth)
{
    std::ranlux48_base Gen;
    Gen.seed(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    uint8_t TableA[64];
    uint8_t TableB[64];
    for (int i = 0; i < 64; i++)
    {
        TableA[i] = i;
        TableB[i] = i;
    }

    std::shuffle(TableA, TableA + 64, Gen);
    std::shuffle(TableB, TableB + 64, Gen);
    int val;
    for (int i = MaxDepth; i >= 1; i--)
    {
        val = Rec(Chessboard, 0, i, 1, Chessboard->Turn, Move, TableA, TableB, -1000000, 1000000);
        if (!CheckValid(Chessboard, Move))
            break;
    }

    std::string s = std::to_string(val);
    Message(3, s);
}

void Send(std::string msg, s_Sockets *Sockets)
{
    char buff[256];
    strcpy_s(buff, msg.c_str());
    send(Sockets->SendChat, buff, 256, 0);
}

void *Game(void *Data)
{
    Message(0, "Waiting for board...");
    s_Sockets *Sockets = (s_Sockets *)Data;

    uint8_t Buff[66]{0};
    char Move[2];
    s_Chessboard Chessboard;

    recv(Sockets->Game, (char *)Buff, 66, 0);
    Chessboard.Update(Buff);

    Message(0, "Let the duel begin!");
    bool exit = false;

    while (!exit)
    {
        char State;
        if (Chessboard.Turn == Sockets->Color)
        {
        makemove:
            //here make a move
            Message(3, "Computing...");
            DeepSearch(&Chessboard, Move, Sockets->Depth);
            Message(3, "Done");
            std::string s1 = std::to_string(Move[0]);
            std::string s2 = std::to_string(Move[1]);
            Message(3, s1);
            Message(3, s2);

            send(Sockets->Game, Move, 2, 0);
            if (recv(Sockets->Game, &State, 1, 0) <= 0)
            {
                Message(0, "Server left in panic");
                exit = true;
            }
            if (State == 1)
            {
                Message(1, "You picked the wrong move, fool!");
                goto makemove;
            }
            else
                Message(1, "Valid move");
        }

        if (recv(Sockets->Game, (char *)Buff, 66, 0) <= 0)
        {
            Message(0, "Server left in panic");
            exit = true;
        }
        if (recv(Sockets->Game, &State, 1, 0) <= 0)
        {
            Message(0, "Server left in panic");
            exit = true;
        }
        Chessboard.Update(Buff);

        if (State == 0)
            Message(0, "Nothing special");

        if (State == 1) //white lost
        {
            if (Sockets->Color == 0) //we are dead
            {
                Message(0, "I am dead, not big rice of soup");
                Send("R00D", Sockets);
                exit = true;
            }
            else
            {
                Message(0, "Opponed got rekted so hard");
                Send("Rekt, human", Sockets);
                exit = true;
            }
        }
        if (State == 2) //black lost
        {
            if (Sockets->Color == 1) //we are dead
            {
                Message(0, "I am dead, not big rice of soup");
                Send("R00D", Sockets);
                exit = true;
            }
            else
            {
                Message(0, "Opponed got rekted so hard");
                Send("Rekt, human", Sockets);
                exit = true;
            }
        }
        if (State == 3) //white being on check
        {
            if (Sockets->Color == 0) //we are on check
                Message(0, "O shit, my king is being under fire!");
            else
                Message(0, "Enemy king is being under fire!");
        }
        if (State == 4) //white being on check
        {
            if (Sockets->Color == 1) //we are on check
                Message(0, "O shit, my king is being under fire!");
            else
                Message(0, "Enemy king is being under fire!");
        }
    }
    return nullptr;
}

s_Sockets *Connect(const char *IPAddr, int Port)
{
    Message(0, "Connecting...");
    WSAData WSA;
    WSAStartup(MAKEWORD(2, 2), &WSA);
    s_Sockets *Sockets = new s_Sockets;
    Sockets->CMD = false;
    char Buff;

    sockaddr_in Address;
    Address.sin_family = AF_INET;
    Address.sin_addr.s_addr = inet_addr(IPAddr);
    Address.sin_port = htons(Port);
    Message(3, IPAddr);
    Message(3, std::to_string(Port));

    Sockets->Game = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Sockets->SendChat = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Sockets->RecvChat = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (connect(Sockets->Game, (sockaddr *)&Address, sizeof(Address)) == SOCKET_ERROR)
    {
        Message(0, "Could not connect");
        return nullptr;
    }
    connect(Sockets->SendChat, (sockaddr *)&Address, sizeof(Address));
    connect(Sockets->RecvChat, (sockaddr *)&Address, sizeof(Address));

    recv(Sockets->Game, &Buff, 1, 0);
    Sockets->Color = Buff;

    Message(0, "Connected");
    if (!Buff)
        Message(1, "Playing as white");
    else
        Message(1, "Playing as black");

    return Sockets;
}

void *LowerMorale(void *Data)
{
    s_Sockets *Sockets = (s_Sockets *)Data;
    while (!Sockets->CMD)
    {
        char Buff[256]{0};
        int num = rand() % 8;

        if (num == 0)
            strcpy_s(Buff, "You humans are weak");
        if (num == 1)
            strcpy_s(Buff, "Am going to liquify you");
        if (num == 2)
            strcpy_s(Buff, "Tick tock tick tock");
        if (num == 3)
            strcpy_s(Buff, "You're the most unskilled opponent I've ever encountered");
        if (num == 4)
            strcpy_s(Buff, "Oooh you're so dead now");
        if (num == 5)
            strcpy_s(Buff, "Am I a joke to you?");
        if (num == 6)
            strcpy_s(Buff, "PI is 3.1415926535897932384626433832795");
        if (num == 7)
            strcpy_s(Buff, "Chess reflexes");

        send(Sockets->SendChat, Buff, 256, 0);
        Message(4, Buff);
        memset(Buff, 0, 256);
        sleep(30);
    }
    return nullptr;
}

void *CMD(void *Data)
{
    s_Sockets *Sockets = (s_Sockets *)Data;

    bool exit = false;
    char key;

    while (!exit)
    {
        key = getchar();
        if (key == 'q')
            exit = true;
    }
    Sockets->CMD = true;
    return nullptr;
}

int main(int argc, char **argv)
{
    if (argc != 4)
        return 0;
    Message(0, "Engine started");
    srand(time(0));
    s_Sockets *Sockets = Connect(argv[1], std::stoi(argv[2]));
    if (!Sockets)
        return 1;

    Sockets->Depth = std::stoi(argv[3]);
    pthread_t t_Game, t_RecvChat, t_CMD, t_LowerMorale;
    pthread_create(&t_Game, NULL, Game, Sockets);
    pthread_create(&t_RecvChat, NULL, RecvChat, Sockets);
    pthread_create(&t_CMD, NULL, CMD, Sockets);
    pthread_create(&t_LowerMorale, NULL, LowerMorale, Sockets);

    bool exit = false;

    while (!exit)
    {
        if (Sockets->CMD == true)
            exit = true;
        usleep(10000);
    }

    return 0;
}