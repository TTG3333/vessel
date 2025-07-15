objects = web.o stringbuilder.o strfuncs.o

web: $(objects)
	gcc -Wall -o web $(objects)

web.o: web.h stringbuilder.h strfuncs.h
stringbuilder.o: stringbuilder.h
strfuncs.o: strfuncs.h

.PHONY: clean
clean:
	rm web $(objects)