#!/bin/bash
bench=linkedlist
optype=balanced

	            	optype=balanced
            		if [ "${bench}" == "linkedlist" ]; then
						echo "optype rate changed"
              		  	optype=regular
              		else
              			optype=balanced
            		fi
	        #optype=balanced
         if [ "${bench}" == "linkedlist" ]; then
				echo "optype changed"
             	optype=regular
           else
           	optype=balanced
         fi
echo ${bench}
echo ${optype}

repeat=2
r_vec={1..${repeat}}
for r in ${r_vec[@]}
do
	echo "${r}"

done