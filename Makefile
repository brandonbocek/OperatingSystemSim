all: oss user log.out

log.out:
	touch log.out

oss:
	gcc -o oss oss.c

user:
	gcc -o user user.c

clean:
	rm oss
	rm user
	rm *.out
