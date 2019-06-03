# Accessing a text file - www.101computing.net/mp3-playlist/
import sys

# inter intra persist wb crit ncrit nat ext

progs = ['linkedlist', 'hashmap', 'bstree',
         'skiplist', 'lfqueue', 'lfqueue2']

infile = open("output/regular/syncbench-mbank-120cycles-nopfixed-8_lfqueue_nmc/data2.txt", "r")
output = open("result/data2" + ".out", "w")

for prog_name in progs:

    # Repeat for each song in the text file
    infile = open("output/regular/syncbench-mbank-120cycles-nopfixed-8_lfqueue_nmc/data2.txt", "r")
    tline = ""
    count = 0
    input_data = []
    input_data2 = []

    for line in infile:

        if count % 5 == 0:
            tline = line
            print(tline)
            input_data = []
            # output.write()
        else:
            if count % 5 == 2:
                fields = line.split(",")
                inter = float(fields[0])
                intra = float(fields[1])
                persist = float(fields[2])
                wb = float(fields[3])
                cwb = float(fields[4])
                ncwb = float(fields[5])
                nat = float(fields[6])
                ext = float(fields[7])

                inter_intra = intra / (inter + intra)
                wb_rate = cwb / wb
                input_data.append(inter_intra)
                input_data.append(wb_rate)

            if count % 5 == 3:
                # This is release persistency
                fields = line.split(",")
                inter = float(fields[0])
                intra = float(fields[1])
                persist = float(fields[2])
                wb = float(fields[3])
                cwb = float(fields[4])
                ncwb = float(fields[5])
                nat = float(fields[6])
                ext = float(fields[7])

                inter_intra = (intra + nat) / (inter + nat + intra)
                wb_rate = cwb / wb
                input_data.append(inter_intra)
                input_data.append(wb_rate)

            #input_data[count-1] = fields[1]
            # output.write(str(input_data[0]))

        if count % 5 == 4:
            tline_fileds = tline.split(",")
            str_line = ""
            if(tline_fileds[0] == prog_name):
                str_line += (tline_fileds[0] + "\t" + tline_fileds[1] + "\t" + tline_fileds[2] + "\t" + str(
                    int(tline_fileds[3])) + "\t" + str(int(tline_fileds[4])) + "\t")
                str_line += str(input_data[0] * 100) + "\t"
                str_line += str(input_data[2] * 100) + "\t"
                str_line += str(input_data[1] * 100) + "\t"
                str_line += str(input_data[3] * 100) + "\t"

                output.write(str_line + "\n")

        count = count + 1


# It is good practice to close the file at the end to free up resources
infile.close()
output.close()

