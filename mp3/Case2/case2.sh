
#!/bin/bash
N=$1
for((i=0; i < N; i++))
do 
    nice ./work 200 R 10000 &
done

