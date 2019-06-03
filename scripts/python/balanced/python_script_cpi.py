# Accessing a text file - www.101computing.net/mp3-playlist/
import sys


progs = ['linkedlist', 'hashmap', 'bstree', 'skiplist', 'lfqueue', 'lfqueue2']
for prog_name in progs:
    infile = open("output/balanced/syncbench-mbank-120cycles-nopfixed-8_lfqueue_nmc/data.txt", "r")
    output = open("result/" + prog_name + ".out", "w")

    # Repeat for each song in the text file
    tline = ""
    no_p = 0.0
    rp = 0.0
    fbep = 0.0
    

    count = 0
    input_data = []

    for line in infile:

        if count % 5 == 0:
            tline = line
            input_data = []
            # output.write()
        else:
            fields = line.split(",")
            input_data.append(float(fields[2]))
            #input_data[count-1] = fields[1]
            # output.write(str(input_data[0]))

        if count % 5 == 4:
            tline_fileds = tline.split(",")
            str_line = ""
            if(tline_fileds[0] == prog_name):
                str_line += (tline_fileds[1] + "\t" + tline_fileds[2] +
                             "\t" + str(int(tline_fileds[3])) + "\t" + str(int(tline_fileds[4])) + "\t")
                
		nop_bep = (float(input_data[0]) - float(input_data[2])) / float(input_data[2])
		rp_bep = (float(input_data[1]) - float(input_data[2])) / float(input_data[2])
		fbrp_bep = (float(input_data[3]) - float(input_data[2])) / float(input_data[2])
               
		rp_nop = (float(input_data[1]) -  float(input_data[0])) / float(input_data[0])
                bep_nop = (float(input_data[2]) - float(input_data[0])) / float(input_data[0])
		fbrp_nop = (float(input_data[3]) - float(input_data[0])) / float(input_data[0])

		nop_fbrp = (float(input_data[0]) -  float(input_data[3])) / float(input_data[3])
                rp_fbrp = (float(input_data[1]) - float(input_data[3])) / float(input_data[3])
		bep_fbrp = (float(input_data[2]) - float(input_data[3])) / float(input_data[3])

                str_line += (str((rp_nop) * 100)) + "\t"		
                str_line += (str((bep_nop) * 100)) + "\t"
		str_line += (str((fbrp_nop) * 100)) + "\t"

                str_line += (str((nop_bep) * 100)) + "\t"
		str_line += (str((rp_bep) * 100)) + "\t"
		str_line += (str((fbrp_bep) * 100)) + "\t"	

 		str_line += (str((nop_fbrp) * 100)) + "\t"
		str_line += (str((rp_fbrp) * 100)) + "\t"
		str_line += (str((bep_fbrp) * 100)) + "\t"	
		

                output.write(str_line + "\n")

        count = count + 1


# It is good practice to close the file at the end to free up resources
infile.close()
output.close()
