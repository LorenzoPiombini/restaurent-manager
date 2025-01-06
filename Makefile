TARGET=bin/restaurant
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))
OBJ_PROD = $(patsubst src/%.c, obj/%_prod.o, $(SRC))

default:file_sys obj_dir bin_dir $(TARGET)

prod: file_sys $(TARGET)_prod 

data:
	rm -f *.dat *.inx

clean:
	rm -f obj/*.o 
	rm -f bin/*
	rm *core*

obj_dir:
	@if [ ! -d ./obj ]; then\
		echo "creating obj directory"\
		mkdir obj ;\
	fi


bin_dir: 
	@if [ ! -d ./bin ]; then\
		echo "creating bin directory";\
		mkdir bin ;\
	fi

file_sys:
	@if [ ! -d "/u" ]; then\
		sudo echo "creating directory and file system... " ; \
		sudo mkdir /u ;\
		sudo touch /u/file_str.txt ;\
		sudo echo "employee|name:t_s:last_name:t_s:shift_id:t_i:role:t_i|0|1" >> /u/file_str.txt ;\
		sudo echo "schedule|date:t_s:shift_id:t_s|52|1" >> /u/file_str.txt ;\
		sudo echo "tips|credit_card:t_f:cash:t_f:date:t_s:service:t_i:employee_id:t_s|365|0" >> /u/file_str.txt ;\
		sudo echo "percentage|value:t_f:role:t_i|0|1" >> /u/file_str.txt ;\
		sudo echo "shift|start_time:t_s:end_time:t_s:role:t_i:employee_id:t_s|0|1" >> /u/file_str.txt ;\
		sudo echo "timecard|clock_in:t_l:clock_out:t_l:employee_id:t_s|365|0" >> /u/file_str.txt ;\
		sudo touch /u/global_file_str.txt ;\
		sudo echo -e "users|user_name:t_s:password:t_s:permission:t_b:employee_id:t_s:restaurant_id:t_i|500|2\n" >> /u/global_file_str ;\
	fi

$(TARGET): $(OBJ)
	gcc -o $@ $? -lssl -lcrypt -luser -lpthread -lque -lbst -lht -lfile -lstrOP -lrecord -lparse -llock -lm -fsanitize=address -fpie -pie -z relro -z now -z noexecstack

obj/%.o : src/%.c
	gcc -Wall -g3 -c $< -o $@ -Iinclude -fstack-protector-strong  -D_FORTIFY_SOURCE=2 -fpie -fPIE -pie -fsanitize=address

$(TARGET)_prod:$(OBJ_PROD)
	gcc -o $@ $? -lssl -lcrypt -luser -lpthread -lque -lbst -lht -lfile -lstrOP -lrecord -lparse -llock -lm -fpie -pie -z relro -z now -z noexecstack

obj/%_prod.o : src/%.c
	gcc -Wall  -c $< -o $@ -Iinclude -fstack-protector-strong  -D_FORTIFY_SOURCE=2 -fPIC

