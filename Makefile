TARGET=bin/restaurant
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

default:$(TARGET)

data:
	rm -f *.dat *.inx

clean:
	rm -f obj/*.o 
	rm -f bin/*
	rm *core*
         
$(TARGET): $(OBJ)
	gcc -o $@ $? -lssl -lcrypt -luser -lpthread -lque -lbst -lht -lfile -lstrOP -lrecord -lparse -llock -lm -fsanitize=address -fpie -pie -z relro -z now -z noexecstack

obj/%.o : src/%.c
	gcc -Wall -g3 -c $< -o $@ -Iinclude -fstack-protector-strong  -D_FORTIFY_SOURCE=2 -fpie -fPIE -pie -fsanitize=address
