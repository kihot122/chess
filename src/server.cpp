#include <pthread.h>
#include <unistd.h>
#include <WinSock2.h>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <mutex>
#include <fstream>
#include <string>

std::mutex m_Message;
std::mutex m_Save;

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
    SOCKET Listen, Client[2], RecvChat[2], SendChat[2];
};

struct s_Chessboard
{
    uint8_t Board[64];
    uint8_t Flag;
    uint8_t Turn;
    s_Chessboard(uint8_t *Data)
    {
        memcpy(Board, Data, 64);
        Flag = Data[64];
        Turn = Data[65];
    }
    s_Chessboard(s_Chessboard &other)
    {
        memcpy(Board, other.Board, 64);
        Flag = other.Flag;
        Turn = other.Turn;
    }
    s_Chessboard()
    {
        memset(Board, 0, 64);

        Board[0] = WHITE_ROOK;
        Board[1] = WHITE_KNIGHT;
        Board[2] = WHITE_BISHOP;
        Board[3] = WHITE_QUEEN;
        Board[4] = WHITE_KING;
        Board[5] = WHITE_BISHOP;
        Board[6] = WHITE_KNIGHT;
        Board[7] = WHITE_ROOK;

        Board[8] = WHITE_PAWN;
        Board[9] = WHITE_PAWN;
        Board[10] = WHITE_PAWN;
        Board[11] = WHITE_PAWN;
        Board[12] = WHITE_PAWN;
        Board[13] = WHITE_PAWN;
        Board[14] = WHITE_PAWN;
        Board[15] = WHITE_PAWN;

        Board[48] = BLACK_PAWN;
        Board[49] = BLACK_PAWN;
        Board[50] = BLACK_PAWN;
        Board[51] = BLACK_PAWN;
        Board[52] = BLACK_PAWN;
        Board[53] = BLACK_PAWN;
        Board[54] = BLACK_PAWN;
        Board[55] = BLACK_PAWN;

        Board[56] = BLACK_ROOK;
        Board[57] = BLACK_KNIGHT;
        Board[58] = BLACK_BISHOP;
        Board[59] = BLACK_QUEEN;
        Board[60] = BLACK_KING;
        Board[61] = BLACK_BISHOP;
        Board[62] = BLACK_KNIGHT;
        Board[63] = BLACK_ROOK;

        Flag = 0;
        Turn = 0;
    }
    void Copy(uint8_t *Dest)
    {
        m_Save.lock();
        memcpy(Dest, Board, 64);
        Dest[64] = Flag;
        Dest[65] = Turn;
        m_Save.unlock();
    }
};

struct s_Shared
{
    s_Sockets *Sockets;
    s_Chessboard *Chessboard;
};

void Message(int source, std::string msg)
{
    m_Message.lock();
    if (source == 0)
        std::cout << "[SERVER]\t" << msg << std::endl;
    if (source == 1)
        std::cout << "[PLAYER 1]\t" << msg << std::endl;
    if (source == 2)
        std::cout << "[PLAYER 2]\t" << msg << std::endl;
    m_Message.unlock();
}

void *RelayChat1(void *Data)
{
    char Buff[256]{0};
    s_Sockets *Sockets = (s_Sockets *)Data;
    while (true)
    {
        if (recv(Sockets->RecvChat[0], Buff, 256, 0) <= 0)
        {
            Message(0, "Player 1 has left the chat, coward");
            return nullptr;
        }
        Message(1, Buff);
        send(Sockets->SendChat[1], Buff, 256, 0);
        memset(Buff, 0, 256);
    }
    return nullptr;
}

void *RelayChat2(void *Data)
{
    char Buff[256];
    s_Sockets *Sockets = (s_Sockets *)Data;
    while (true)
    {
        if (recv(Sockets->RecvChat[1], Buff, 256, 0) <= 0)
        {
            Message(0, "Player 2 has left the chat, coward");
            return nullptr;
        }
        Message(2, Buff);
        send(Sockets->SendChat[0], Buff, 256, 0);
        memset(Buff, 0, 256);
    }
    return nullptr;
}

s_Sockets *Lobby()
{
    Message(0, "Starting lobby thread...");

    WSAData WSA;
    WSAStartup(MAKEWORD(2, 2), &WSA);
    s_Sockets *Sockets = new s_Sockets;
    char Buff;

    Sockets->Listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in Address;
    Address.sin_family = AF_INET;
    Address.sin_addr.s_addr = inet_addr("127.0.0.1");
    Address.sin_port = htons(8303);

    if (bind(Sockets->Listen, (sockaddr *)&Address, sizeof(Address)) == SOCKET_ERROR)
        Message(0, "Bind error");

    if (listen(Sockets->Listen, 1) == SOCKET_ERROR)
        Message(0, "Listen error");

    Message(0, "Waiting for players");

    for (int i = 0; i < 2; i++)
    {
        Sockets->Client[i] = SOCKET_ERROR;
        Sockets->RecvChat[i] = SOCKET_ERROR;
        Sockets->SendChat[i] = SOCKET_ERROR;
        while (Sockets->Client[i] == SOCKET_ERROR)
            Sockets->Client[i] = accept(Sockets->Listen, NULL, NULL);
        while (Sockets->RecvChat[i] == SOCKET_ERROR)
            Sockets->RecvChat[i] = accept(Sockets->Listen, NULL, NULL);
        while (Sockets->SendChat[i] == SOCKET_ERROR)
            Sockets->SendChat[i] = accept(Sockets->Listen, NULL, NULL);

        Buff = i;
        send(Sockets->Client[i], &Buff, 1, 0);
        Message(0, "Player connected");
    }
    Message(0, "Starting server thread...");
    return Sockets;
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

uint8_t CheckVictory(s_Chessboard *Chessboard)
{
    if (KingCheck(Chessboard)) //checking if king has any possible move to avoid death
    {
        s_Chessboard *Temp = new s_Chessboard(*Chessboard); //make shadow copy
        char Move[2];
        for (int i = 0; i < 64; i++)
        {
            Move[0] = i;
            for (int j = 0; j < 64; j++)
            {
                Move[1] = j;
                if (!CheckValid(Temp, Move))
                {
                    delete Temp;
                    return Chessboard->Turn + 3;
                }
            }
        }
        //did not found any legal moves, game over
        delete Temp;
        return Chessboard->Turn + 1;
    }

    return 0;
}

bool CheckPat(s_Chessboard *Chessboard)
{
    char move[2];
    for (int i = 0; i < 64; i++)
    {
        move[0] = i;
        for (int j = 0; j < 64; j++)
        {
            move[1] = j;
            s_Chessboard *Temp = new s_Chessboard(*Chessboard);
            if (!CheckValid(Temp, move)) //not pat
            {
                delete Temp;
                return 0;
            }
            delete Temp;
        }
    }
    return 1; //did not find any legal moves
}

void *ServerThread(void *Data)
{
    s_Shared *Shared = (s_Shared *)Data;
    uint8_t Buff[66];
    Shared->Chessboard->Copy(Buff);

    send(Shared->Sockets->Client[0], (char *)Buff, 66, 0);
    send(Shared->Sockets->Client[1], (char *)Buff, 66, 0);

    char Move[2];
    char State;
    bool exit = false;
    while (!exit)
    {
    Turn:
        if (Shared->Chessboard->Turn == 0)
        {
            Message(0, "White turn");
            if (recv(Shared->Sockets->Client[0], Move, 2, 0) <= 0)
            {
                Message(0, "Player 1 has fled the combat, coward");
                return nullptr;
            }

            if (CheckValid(Shared->Chessboard, Move))
            {
                Message(0, "Wait, that's illegal!");
                State = true;
                send(Shared->Sockets->Client[0], &State, 1, 0);
                goto Turn;
            }
            else
            {
                Message(0, "Ok move");
                State = false;
                send(Shared->Sockets->Client[0], &State, 1, 0);
            }
        }
        else
        {
            Message(0, "Black turn");
            if (recv(Shared->Sockets->Client[1], Move, 2, 0) <= 0)
            {
                Message(0, "Player 2 has fled the combat, coward");
                return nullptr;
            }

            if (CheckValid(Shared->Chessboard, Move))
            {
                Message(0, "Wait, that's illegal!");
                State = true;
                send(Shared->Sockets->Client[1], &State, 1, 0);
                goto Turn;
            }
            else
            {
                Message(0, "Ok move, next");
                State = false;
                send(Shared->Sockets->Client[1], &State, 1, 0);
            }
        }

        Shared->Chessboard->Copy(Buff);
        send(Shared->Sockets->Client[0], (char *)Buff, 66, 0);
        send(Shared->Sockets->Client[1], (char *)Buff, 66, 0);

        State = CheckVictory(Shared->Chessboard);
        if (State == 0)
            State = CheckPat(Shared->Chessboard);

        if (State == 0)
            Message(0, "Next turn");

        if (State == 1) //white king is being check-mated (?)
        {
            Message(0, "Game over, black wins!");
            exit = true;
        }

        if (State == 2) //black king is being check-mated
        {
            Message(0, "Game over, white wins!");
            exit = true;
        }

        if (State == 3)
            Message(0, "Black made a check!");

        if (State == 4)
            Message(0, "White made a check!");

        if (State == 5)
            Message(0, "DRAW");

        send(Shared->Sockets->Client[0], &State, 1, 0);
        send(Shared->Sockets->Client[1], &State, 1, 0);
    }

    return nullptr;
}

int main(int argc, char **argv)
{
    s_Chessboard *Chessboard;
    if (argc == 2)
    {
        Message(0, "Loading saved duel");
        uint8_t Buff[66]{0};
        std::fstream Stream(argv[1], std::fstream::binary | std::fstream::in);
        if (!Stream.is_open())
        {
            Message(0, "Could not load save file");
            return 0;
        }
        Stream.read((char *)Buff, 66);
        Chessboard = new s_Chessboard(Buff);
        Stream.close();
        Message(0, "Loaded");
    }
    else
        Chessboard = new s_Chessboard();

    s_Sockets *Sockets = Lobby();

    s_Shared *Shared = new s_Shared;
    Shared->Chessboard = Chessboard;
    Shared->Sockets = Sockets;

    pthread_t t_ServerThread, t_RelayChat1, t_RelayChat2;
    pthread_create(&t_ServerThread, NULL, ServerThread, Shared);
    pthread_create(&t_RelayChat1, NULL, RelayChat1, Sockets);
    pthread_create(&t_RelayChat2, NULL, RelayChat2, Sockets);

    bool exit = false;
    char key;
    while (!exit)
    {
        key = getchar();
        if (key == 'q')
            return 0;
        if (key == 's')
        {
            char Path[256];
            std::cin.getline(Path, 256);
            std::fstream Stream(Path + 1, std::fstream::binary | std::fstream::out);

            uint8_t Buff[66];
            Chessboard->Copy(Buff);
            Stream.write((char *)Buff, 66);
            Stream.close();
            Message(0, "Game saved");
        }
    }
    return 0;
}