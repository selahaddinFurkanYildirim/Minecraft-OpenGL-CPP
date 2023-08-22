linux:
	clear
	g++ -g --std=c++17 -I src/ src/* -o bin/main -lSDL2main -lSDL2 -Wall -lGL -lGLU -lglut -lGLEW -lglfw -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lXinerama -lXcursor
	cd bin && ./main &
