TARGET=bin/restaurant
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))
OBJ_PROD = $(patsubst src/%.c, obj/%_prod.o, $(SRC))

default:$(TARGET)

prod:$(TARGET)_prod 

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

$(TARGET)_prod:$(OBJ_PROD)
	gcc -o $@ $? -lssl -lcrypt -luser -lpthread -lque -lbst -lht -lfile -lstrOP -lrecord -lparse -llock -lm -fpie -pie -z relro -z now -z noexecstack

obj/%_prod.o : src/%.c
	gcc -Wall  -c $< -o $@ -Iinclude -fstack-protector-strong  -D_FORTIFY_SOURCE=2 -fPIC
