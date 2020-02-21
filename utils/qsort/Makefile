CC          = gcc

test_qsort: test_qsort.c ../scarab_markers.h libc_qsort.h libc_qsort.c
	$(CC) -o test_qsort test_qsort.c libc_qsort.c -O2 -static

clean:
	rm -f test_qsort


