#include <pthread.h>
#include <unistd.h>
#include <mutex>
#include <iostream>
#include <fstream>
#include <string>
#include <WinSock2.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"

std::mutex m_Message;
std::mutex m_Paint;

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
};

struct s_Chessboard
{
    uint8_t Board[64];
    uint8_t Flag;
    uint8_t Turn;

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
        std::cout << "[SERVER]\t" << msg << std::endl;
    if (source == 1)
        std::cout << "[PLAYER]\t" << msg << std::endl;
    if (source == 2)
        std::cout << "[OPPONENT]\t" << msg << std::endl;
    if (source == 3)
        std::cout << "[CLIENT]\t" << msg << std::endl;
    if (source == 4)
        std::cout << "[DEBUG]\t" << msg << std::endl;
    m_Message.unlock();
}

struct s_Render
{
    s_Chessboard Chessboard;
    SDL_Window *Window;
    SDL_Renderer *Rend;
    SDL_Texture *Atlas[17];
    SDL_Event E;
    int32_t Selection[2];
    uint8_t check;
    bool sel;

    s_Render()
    {
        SDL_Init(SDL_INIT_EVERYTHING);
        IMG_Init(IMG_INIT_PNG);

        Window = SDL_CreateWindow("Not this again", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 800, SDL_WINDOW_SHOWN);
        Rend = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);

        Atlas[0] = IMG_LoadTexture(Rend, "../data/white_pawn.png");
        Atlas[1] = IMG_LoadTexture(Rend, "../data/white_knight.png");
        Atlas[2] = IMG_LoadTexture(Rend, "../data/white_bishop.png");
        Atlas[3] = IMG_LoadTexture(Rend, "../data/white_rook.png");
        Atlas[4] = IMG_LoadTexture(Rend, "../data/white_queen.png");
        Atlas[5] = IMG_LoadTexture(Rend, "../data/white_king.png");

        Atlas[6] = IMG_LoadTexture(Rend, "../data/black_pawn.png");
        Atlas[7] = IMG_LoadTexture(Rend, "../data/black_knight.png");
        Atlas[8] = IMG_LoadTexture(Rend, "../data/black_bishop.png");
        Atlas[9] = IMG_LoadTexture(Rend, "../data/black_rook.png");
        Atlas[10] = IMG_LoadTexture(Rend, "../data/black_queen.png");
        Atlas[11] = IMG_LoadTexture(Rend, "../data/black_king.png");

        Atlas[12] = IMG_LoadTexture(Rend, "../data/ghost.png");

        Atlas[13] = IMG_LoadTexture(Rend, "../data/white_square.png");
        Atlas[14] = IMG_LoadTexture(Rend, "../data/black_square.png");
        Atlas[15] = IMG_LoadTexture(Rend, "../data/selection.png");
        Atlas[16] = IMG_LoadTexture(Rend, "../data/red_evil_square.png");

        Selection[0] = -1;
        Selection[1] = -1;
        sel = false;
        check = 0;
    }
    void Paint()
    {
        m_Paint.lock();

        SDL_SetRenderDrawColor(Rend, 255, 255, 255, 255);
        SDL_RenderClear(Rend);

        bool f = 0;
        SDL_Rect R;

        for (int x = 0; x < 8; x++)
        {
            for (int y = 0; y < 8; y++)
            {
                f = !f;
                R.x = x * 100;
                R.y = 700 - y * 100;
                R.w = 100;
                R.h = 100;
                if (!f)
                    SDL_RenderCopy(Rend, Atlas[13], NULL, &R);
                else
                    SDL_RenderCopy(Rend, Atlas[14], NULL, &R);
            }
            f = !f;
        }
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
            {
                R.x = x * 100;
                R.y = 700 - y * 100;
                R.w = 100;
                R.h = 100;

                if (Chessboard.Board[y * 8 + x])
                    SDL_RenderCopy(Rend, Atlas[Chessboard.Board[y * 8 + x] - 1], NULL, &R);
            }
        for (int i = 0; i < 2; i++)
        {
            int Temp[2] = {Selection[0], Selection[1]};
            if (Temp[i] != -1)
            {
                R.x = (Temp[i] % 8) * 100;
                R.y = 700 - ((Temp[i] / 8) * 100);
                R.w = 100;
                R.h = 100;
                SDL_RenderCopy(Rend, Atlas[15], NULL, &R);
            }
        }
        if (check)
        {
            for (int i = 0; i < 64; i++)
            {
                if ((check == 1 && Chessboard.Board[i] == WHITE_KING) ||
                    (check == 2 && Chessboard.Board[i] == BLACK_KING))
                {
                    R.x = (i % 8) * 100;
                    R.y = 700 - (i / 8) * 100;
                    R.w = 100;
                    R.h = 100;
                    SDL_RenderCopy(Rend, Atlas[16], NULL, &R);
                }
            }
        }
        SDL_RenderPresent(Rend);
        m_Paint.unlock();
    }

    bool Event()
    {
        while (SDL_PollEvent(&E))
        {
            if (E.type == SDL_QUIT)
                return true;
            if (E.type == SDL_MOUSEBUTTONDOWN && E.button.button == SDL_BUTTON_LEFT)
            {
            Select:

                if (Selection[0] == -1)
                    Selection[0] = ((800 - E.button.y) / 100) * 8 + E.button.x / 100;
                else if (Selection[1] == -1)
                {
                    Selection[1] = ((800 - E.button.y) / 100) * 8 + E.button.x / 100;
                    sel = true;
                }
                else
                {
                    Selection[0] = -1;
                    Selection[1] = -1;
                    sel = false;
                    goto Select;
                }
                Paint();
            }
            if (E.type == SDL_MOUSEBUTTONDOWN && E.button.button == SDL_BUTTON_RIGHT)
            {
                Selection[0] = -1;
                Selection[1] = -1;
                sel = false;
                Paint();
            }
        }
        return false;
    }

    void RetMove(char *Dest)
    {
        Dest[0] = (char)Selection[0];
        Dest[1] = (char)Selection[1];
    }
};

struct s_Shared
{
    s_Sockets *Sockets;
    s_Render *Render;
};

void *RecvChat(void *Data)
{
    char Buff[256]{0};
    s_Sockets *Sockets = (s_Sockets *)Data;
    while (true)
    {
        if (recv(Sockets->RecvChat, Buff, 256, 0) <= 0)
        {
            Message(3, "Server left the chat...or something");
            return nullptr;
        }
        Message(2, Buff);
        memset(Buff, 0, 256);
    }
    return nullptr;
}

s_Sockets *Connect(const char *IPAddr, int Port)
{
    Message(3, "Connecting...");
    WSAData WSA;
    WSAStartup(MAKEWORD(2, 2), &WSA);
    s_Sockets *Sockets = new s_Sockets;
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
        Message(3, "Could not connect");
        return nullptr;
    }
    connect(Sockets->SendChat, (sockaddr *)&Address, sizeof(Address));
    connect(Sockets->RecvChat, (sockaddr *)&Address, sizeof(Address));

    recv(Sockets->Game, &Buff, 1, 0);
    Sockets->Color = Buff;

    Message(3, "Connected");
    if (!Buff)
        Message(0, "Playing as white");
    else
        Message(0, "Playing as black");

    return Sockets;
}

void *Game(void *Data)
{
    Message(3, "Waiting for board...");
    s_Shared *Shared = (s_Shared *)Data;

    uint8_t Buff[66]{0};
    char Move[2];

    recv(Shared->Sockets->Game, (char *)Buff, 66, 0);
    Shared->Render->Chessboard.Update(Buff);
    Shared->Render->Paint();
    Message(3, "Let the duel begin!");
    bool exit = false;

    while (!exit)
    {
        char State;
        if (Shared->Render->Chessboard.Turn == Shared->Sockets->Color)
        {
        makemove:

            while (!Shared->Render->sel)
                usleep(10000);
            Shared->Render->sel = false;
            Shared->Render->RetMove(Move);
            send(Shared->Sockets->Game, Move, 2, 0);

            if (recv(Shared->Sockets->Game, &State, 1, 0) <= 0)
            {
                Message(3, "Server has left the game, ok");
                exit = true;
            }

            if (State == 1)
            {
                Message(0, "You picked the wrong move, fool!");
                goto makemove;
            }
            else
                Message(0, "Valid move");
        }
        if (recv(Shared->Sockets->Game, (char *)Buff, 66, 0) <= 0)
        {
            Message(3, "Server has left the game, ok");
            exit = true;
        }
        Shared->Render->Chessboard.Update(Buff);
        Shared->Render->Paint();
        if (recv(Shared->Sockets->Game, &State, 1, 0) <= 0)
        {
            Message(3, "Server has left the game, ok");
            exit = true;
        }
        if (State == 0)
            Shared->Render->check = false;

        if (State == 1) //white lost
        {
            Shared->Render->check = 1;
            if (Shared->Sockets->Color == 0) //we are dead
            {
                Message(3, "You are dead, not big rice of soup");
                exit = true;
            }
            else
            {
                Message(3, "Opponed got rekted so hard");
                exit = true;
            }
        }
        if (State == 2) //black lost
        {
            Shared->Render->check = 2;
            if (Shared->Sockets->Color == 1) //we are dead
            {
                Message(3, "You are dead, not big rice of soup");
                exit = true;
            }
            else
            {
                Message(3, "Opponed got rekted so hard");
                exit = true;
            }
        }
        if (State == 3) //white being on check
        {
            Shared->Render->check = 1;
            if (Shared->Sockets->Color == 0) //we are on check
                Message(3, "Your king is being under fire!");
            else
                Message(3, "Enemy king is being under fire!");
        }
        if (State == 4) //white being on check
        {
            Shared->Render->check = 2;
            if (Shared->Sockets->Color == 1) //we are on check
                Message(3, "Your king is being under fire!");
            else
                Message(3, "Enemy king is being under fire!");
        }
        if(State == 5) //draw
        {
            Shared->Render->check = false;
            Message(3, "DRAW");
            exit = true;
        }

        Shared->Render->Paint();
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
        if (key == 's')
        {
            char Buff[256];
            std::cin.getline(Buff, 256);
            send(Sockets->SendChat, Buff, 256, 0);
            Message(1, Buff);
        }
    }
    Sockets->CMD = true;
    return nullptr;
}

int main(int argc, char **argv)
{
    s_Sockets *Sockets = Connect(argv[1], std::stoi(argv[2]));
    if (!Sockets)
        return 1;

    s_Render *Render = new s_Render;
    s_Shared *Shared = new s_Shared;
    Shared->Sockets = Sockets;
    Shared->Render = Render;
    Sockets->CMD = false;

    pthread_t t_Game, t_RecvChat, t_CMD;
    pthread_create(&t_Game, NULL, Game, Shared);
    pthread_create(&t_RecvChat, NULL, RecvChat, Sockets);
    pthread_create(&t_CMD, NULL, CMD, Sockets);

    bool exit = false;

    while (!exit)
    {
        if (Render->Event())
            exit = true;
        if (Sockets->CMD == true)
            exit = true;
        usleep(10000);
    }

    return 0;
}