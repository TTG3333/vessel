objects = web.o stringbuilder.o

web: $(objects)
	gcc -Wall -o web $(objects)

web.o: web.h stringbuilder.h
stringbuilder.o: stringbuilder.h

.PHONY: clean
clean:
	rm web $(objects)