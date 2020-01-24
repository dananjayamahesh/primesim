#!/usr/bin/python
# -*- coding: utf-8 -*-
# Accessing a text file - www.101computing.net/mp3-playlist/

#Remember: 38 does not have a piggybacking.
import sys

def main():

	infile_path = sys.argv[1]
	print(infile_path)
	outfile_path = sys.argv[2]
	print(outfile_path)

	progs = [
		'linkedlist',
		'hashmap',
		'bstree',
		'skiplist',
		'lfqueue',
		'lfqueue2',
		]
	
	#out_exec = open('../asplos-output/cached/syncbench-mbank-120cycles/exec_time.out', 'w+')
	out_exec = open(outfile_path, 'w+')
	out_exec.write('benchmarks-T \t ,sb \t ,bb \t ,bb+pf \t ,lrp \t ,lrp+pf \n')
	#out_exec.write('benchmark-T \t ,sb \t ,bb \t ,lrp\n')
	
	for prog_name in progs:
		#infile = open('../asplos-output/cached/syncbench-mbank-120cycles/data_cpi.txt', 'r')
		infile = open(infile_path, 'r')
		#output = open('../asplos-output/cached/syncbench-mbank-120cycles/' + prog_name + '-exec.out', 'w+')
	
		# Repeat for each song in the text file
	
		tline = ''
		no_p = 0.0
		rp = 0.0
		fbep = 0.0
	
		count = 0
		input_data = []
	
		for line in infile:
	
			if count % 7 == 0:
				tline = line
				input_data = []
			else:
	
				# output.write()
	
				fields = line.split(',')
				input_data.append(float(fields[2]))
	
				# input_data[count-1] = fields[1]
				# output.write(str(input_data[0]))
				# Order: 0, 8, 3, 7, 4, 6
	
			if count % 7 == 6:
	
				tline_fileds = tline.split(',')
				str_line = ''
				exec_time_line=''
				if tline_fileds[0] == prog_name:
					str_line += tline_fileds[1] + '\t' + tline_fileds[2] \
						+ '\t' + str(int(tline_fileds[3])) + '\t' \
						+ str(int(tline_fileds[4])) + ' \t'
	 
					exec_time_line += tline_fileds[0] + '-' + tline_fileds[1] + '   \t'
			
					rppf_nop = (float(input_data[1]) - float(input_data[0])) \
						/ float(input_data[0])
					rp_nop = (float(input_data[2]) - float(input_data[0])) \
						/ float(input_data[0])
					beppf_nop = (float(input_data[3]) - float(input_data[0])) \
						/ float(input_data[0])
					bep_nop = (float(input_data[4]) - float(input_data[0])) \
						/ float(input_data[0])
					fbrp_nop = (float(input_data[5]) - float(input_data[0])) \
						/ float(input_data[0])				
	
					str_line += str(abs(rppf_nop * 100)) + '\t'				
					str_line += str(abs(rp_nop * 100)) + '\t'
					str_line += str(abs(beppf_nop * 100)) + '\t'
					str_line += str(abs(bep_nop * 100)) + '\t'
					str_line += str(abs(fbrp_nop * 100)) + '\t'
	
					
					exec_time_line += ','+ str((abs(fbrp_nop * 100)+100)/100) + ' \t'
					exec_time_line += ','+  str((abs(bep_nop * 100)+100)/100) + ' \t'
					exec_time_line += ','+  str((abs(beppf_nop * 100)+100)/100) + ' \t'
					exec_time_line += ','+  str((abs(rp_nop * 100)+100)/100) + ' \t'
					exec_time_line += ','+ str((abs(rppf_nop * 100)+100)/100) + ' \t'	
					
									
			
					#output.write(str_line + '\n')
	
					out_exec.write(exec_time_line + '\n');
					print(tline_fileds[0] +'-'+ str_line)
		
			count = count + 1
	
	# It is good practice to close the file at the end to free up resources
	
	infile.close()
	#output.close()

main()