all:
	@echo 'build - to build a program'
	@echo 'run - to run a program'
	@echo 'clean - to clear directory from temporary files'

build:
	gcc ntfs.c -o ntfs
run:
	@./ntfs
clean:
	@rm ntfs
