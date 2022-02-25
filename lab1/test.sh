#!/bin/bash
pkt_num=0;
round=100;
if [ $# -eq 1 ]
then 
  round=$1;
fi
success_round=0;
for i in $(seq $round)
do 
echo -e "\n" | ./rdt_sim 1000 0.1 100 0.15 0.15 0.15 0 &> tmp.txt
result=`grep "packets" tmp.txt | awk '{print $1;}'`;
pkt_num=`expr $result + $pkt_num`;
result=$(grep "Congratulations" tmp.txt)
if [ ${#result} -ne 0 ]
then success_round=`expr $success_round + 1`;
fi  
done
avg=`expr $pkt_num / $round`;
echo "case 1 (./rdt_sim 1000 0.1 100 0.15 0.15 0.15 0) avg: $avg";
echo "Success ratio $success_round/$round"

success_round=0;
pkt_num=0;
for i in $(seq $round)
do 
echo -e "\n" | ./rdt_sim 1000 0.1 100 0.3 0.3 0.3 0 &> tmp.txt
result=`grep "packets" tmp.txt | awk '{print $1;}'`;
pkt_num=`expr $result + $pkt_num`;
result=$(grep "Congratulations" tmp.txt)
if [ ${#result} -ne 0 ]
then success_round=`expr $success_round + 1`;
else
cat tmp.txt &> error.txt;
fi  
done
avg=`expr $pkt_num / $round`;
echo "case 2(./rdt_sim 1000 0.1 100 0.3 0.3 0.3 0) avg: $avg";
echo "Success ratio $success_round/$round"

rm -f tmp.txt
