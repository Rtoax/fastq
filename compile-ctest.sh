#!/bin/bash
# 荣涛 2021年1月27日

rm -f *.out

redis_dict_dir="../../redis/dict/6.2.1/"
redis_dict_srcs=(dict.c  mt19937-64.c  sds.c  siphash.c  zmalloc.c)
redis_dict_objs=""
for dict in ${redis_dict_srcs[@]}
do 
	echo "Compile $dict -> ${dict%.*}.o"
	gcc $redis_dict_dir/$dict -c -o ${dict%.*}.o -I$redis_dict_dir -ltcmalloc -g
	redis_dict_objs="$redis_dict_objs ${dict%.*}.o"
done

echo "Redis Dict objects : $redis_dict_objs"

LIBS="$redis_dict_objs fastq.c -lcrypt -pthread -I./ -I$redis_dict_dir -ltcmalloc -g"

#if [ $# -lt 1 ]; then
#	echo "$0 [program source file]"
#	exit 1
#fi

#file=$1
# (test-0.c test-1.c test-2.c test-3.c test-4.c test-5.c)
# 
test_files=(test.c )
for file in ${test_files[@]}
do 
	echo "Compile $file -> ${file%.*}.out"
#	gcc $file $LIBS -o ${file%.*}.epoll.stat.out -w $* -D_FASTQ_EPOLL=1 -D_FASTQ_STATS=1
	gcc $file $LIBS -o ${file%.*}.epoll.out -w $* -D_FASTQ_EPOLL=1 -g -ggdb
#	gcc $file $LIBS -o ${file%.*}.select.stat.out -w $* -D_FASTQ_SELECT=1  -D_FASTQ_STATS=1
	gcc $file $LIBS -o ${file%.*}.select.out -w $* -D_FASTQ_SELECT=1 -g -ggdb
done


