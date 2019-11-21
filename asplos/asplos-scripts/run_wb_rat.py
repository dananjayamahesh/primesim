# Accessing a text file - www.101computing.net/mp3-playlist/
import sys

# inter intra persist wb crit ncrit nat ext

progs = ['linkedlist', 'hashmap', 'bstree',
         'skiplist', 'lfqueue', 'lfqueue2']

bench=['NULL','LRP_PF', 'LRP', 'BB+PF', 'BB', 'SB']

input_path=""
output_path="../asplos-output/cached/syncbench-mbank-120cycles-A/data_meta.txt"
#"../asplos-output/cached/syncbench-mbank-120cycles/data_meta.txt"
infile = open(output_path, "r")
output = open("../asplos-result/cached/data2" + ".out", "w")
output_wb = open("../asplos-result/cached/data_wb" + ".out", "w")
output_wb_all = open("../asplos-result/cached/data_wb_all" + ".out", "w")
output_wb_tot = open("../asplos-result/cached/data_wb_tot" + ".out", "w")

#output_wb_conflicts = open("result/data_wb_conflicts" + ".out", "w")

for prog_name in progs:

    # Repeat for each song in the text file
    infile = open(output_path, "r")
    tline = ""
    count = 0
    input_data = []
    input_data2 = []
    str_wb_line = ""
    str_wb_all_line = ""
    str_wb_tot_line = ""
    #effective_ratio = 0.8
    str_lrp_line = ""
    #lrp_ratio = 0.5
    pf_ratio = 0.5

    for line in infile:

        if count % 7 == 0:
            tline = line
            #print(tline)
            input_data = []
            str_wb_line = ""
            str_wb_all_line = ""
            str_wb_tot_line = ""
            

            # output.write()
        else:
            if (count % 7 == 2) or (count % 7 == 3) or (count % 7 == 4) or (count % 7 == 5):
                
                persist_model = 0 if ((count % 7 == 2) or (count % 7 == 3)) else 1

                fields = line.split(",")

                clwb_tot = float(fields[0])
                critical_clwb = float(fields[1])
                noncritical_clwb = float(fields[3])
                natural_clwb = float(fields[4])
                all_natural_clwb = float(fields[5])

                intra_tot = float(fields[6])
                intra_vis_conflicts_tot = float(fields[7])
                intra_evi_conflicts_tot = float(fields[8])
                intra_evi_M_conflicts_tot = float(fields[9]) #Inter

                intra_allconflicts_tot = float(fields[10])
                intra_vis_allconflicts_tot = float(fields[11])
                intra_evi_allconflicts_tot = float(fields[12])
                intra_evi_M_allconflicts_tot = float(fields[13])
                intra_M_allconflicts_tot = float(fields[14])

                intra_persist_tot = float(fields[15])
                intra_vis_persists_tot = float(fields[16])
                intra_evi_persists_tot = float(fields[17])
                intra_evi_M_persists_tot = float(fields[18])
                
                inter_tot = float(fields[19])
                inter_M_conflicts_tot = float(fields[20])
                inter_allconflicts_tot = float(fields[21])
                inter_M_allconflicts_tot = float(fields[22])
                inter_persist_tot = float(fields[23])
                inter_M_persists_tot = float(fields[24])

                all_persists_tot = float(fields[25])
                all_pf_persists_tot = float(fields[26])
                vis_pf_persists_tot = float(fields[27])
                evi_pf_persists_tot = float(fields[28])
                inter_pf_persists_tot = float(fields[29])

                #additional inter-conflict sources.
                invals_tot = float(fields[30])
                invals_M_tot = float(fields[31])
                invals_E_tot = float(fields[32])
                shares_tot = float(fields[33])
                shares_M_tot = float(fields[34])
                shares_E_tot = float(fields[35])

                conflict_evi_persists = float(fields[36])
                eviction_conflicts =  float(fields[38])

                inter_intra = intra_tot / (inter_tot + intra_tot)
                wb_rate = critical_clwb / clwb_tot
                input_data.append(inter_intra)
                input_data.append(wb_rate)

                #inter_intra = (intra + nat) / (inter + nat + intra)
                #wb_rate = cwb / wb
                #input_data.append(inter_intra)
                #input_data.append(wb_rate)

                #Stats necessary for the 
                #intra_all = intra_persist_tot+intra_vis_conflicts_tot+all_natural_clwb
                #intra_crit_all =  intra_persist_tot+intra_vis_conflicts_tot+(natural_clwb*0.5 if (persist_model==0) else all_natural_clwb*0.8)
                
                intra_all = intra_persist_tot+intra_vis_conflicts_tot+all_natural_clwb + (0 if (persist_model==0) else natural_clwb)
                intra_crit_all =  intra_persist_tot+intra_vis_conflicts_tot+conflict_evi_persists
                
                non_conflict_evictions = intra_all-intra_crit_all

                #after PF:
                #Only for the proactive flushing
                intra_crit_all = intra_crit_all- ((vis_pf_persists_tot+evi_pf_persists_tot)*0.9)

                inter_M_all = inter_persist_tot+invals_M_tot+shares_M_tot
                inter_all = inter_persist_tot+invals_tot+shares_tot
                #inter_crit_all = inter_persist_tot #inter is considered as non-critical here. so no affect of PF.
                
                intra_vis_all = intra_vis_conflicts_tot+intra_vis_persists_tot #Totoal visibility conflicts.
                intra_evi_all = intra_evi_persists_tot+ conflict_evi_persists #Total Eviction OCnflicts

                #inter_crit_all = inter_persist_tot-(inter_pf_persists_tot*0.50)
                
                #Proactive FLushing Stats
                if(persist_model==1):
                    inter_crit_all = inter_persist_tot-(inter_pf_persists_tot*0)
                else:
                    inter_crit_all = inter_persist_tot-(inter_pf_persists_tot*0.5)

                #all_wb = intra_all+inter_M_all
                all_wb = intra_all+inter_M_all
                crit_ration = (intra_crit_all+inter_crit_all)/all_wb
                crit_ration2 = (intra_crit_all)/all_wb

                #with proactive flushing?.
                #str(crit_ration2*100)+ "\t"+ 
                str_wb_all_line = str(crit_ration2*100)+ "\t"+str_wb_all_line
                
                intra_inter_ratio = (intra_tot*100)/(intra_tot+inter_tot)
                intra_inter_crit_ratio = (intra_crit_all*100)/(intra_crit_all+inter_crit_all)

                str_wb_line += str(count % 7) + '\t' + str(intra_inter_ratio) +"\t" + str(intra_inter_crit_ratio)+ "\n"
                #str_wb_line += str(intra_tot) + "\t" + str(inter_tot) + "\t" + str(intra_inter_ratio) +"\t" + "\n"
                #str_wb_line += str(fields[0]) + "\t" + str(fields[1]) + "\t" + str(fields[2]) + "\t" + str(fields[3]) + "\t" + str(fields[4]) + "\t" + str(fields[5]) + "\t" + str(fields[6]) + "\t" + str(fields[7]) + "\t" + str(fields[8]) + "\t" + str(fields[9]) + "\t" + str(fields[10]) + "\n"

                #str_wb_tot_line = str(all_wb)+ "\t"+str_wb_tot_line
                intra_vis_ratio = (intra_vis_persists_tot+intra_vis_conflicts_tot)/intra_all
                intra_evi_ratio =  ( intra_evi_persists_tot+ conflict_evi_persists) / intra_all
                
                str_wb_tot_line = str(intra_vis_ratio*100)+ "\t"+ str(intra_evi_ratio*100)+ "\t"+str_wb_tot_line

        if count % 7 == 6:
            tline_fileds = tline.split(",")
            str_line = ""
            if(tline_fileds[0] == prog_name):
                str_line += (tline_fileds[0] + "\t" + tline_fileds[1] + "\t" + tline_fileds[2] + "\t" + str(
                    int(tline_fileds[3])) + "\t" + str(int(tline_fileds[4])) + "\t")
                #str_wb_line += (tline_fileds[0] + "\t" + tline_fileds[1] + "\t" + tline_fileds[2] + "\t" + str(
                #    int(tline_fileds[3])) + "\t" + str(int(tline_fileds[4])) + "\t")

                str_line += str(input_data[0] * 100) + "\t"
                str_line += str(input_data[2] * 100) + "\t"
                str_line += str(input_data[1] * 100) + "\t"
                str_line += str(input_data[3] * 100) + "\t"

                output.write(str_line + "\n")
                output_wb.write(str_wb_line + "\n")
                output_wb_all.write(str_wb_all_line + "\n")
                #print(prog_name+"\t"+str_wb_all_line + "\n")
                print(prog_name+"\t"+str_wb_all_line)
                
                output_wb_tot.write(str_wb_tot_line + "\n")

        count = count + 1

# It is good practice to close the file at the end to free up resources
infile.close()
output.close()
output_wb.close()
output_wb_all.close()
output_wb_tot.close()

