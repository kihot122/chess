g++ -O3 -march=native -c ./src/server.cpp -o ./obj/server.obj
g++ -O3 -march=native -c ./src/client.cpp -o ./obj/client.obj
g++ -O3 -march=native -c ./src/engine.cpp -o ./obj/engine.obj

g++ ./obj/server.obj -L./lib -lws2_32 -o ./bin/server.exe
g++ ./obj/client.obj -L./lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lws2_32 -o ./bin/client.exe
g++ ./obj/engine.obj -L./lib -lws2_32 -o ./bin/engine.exe