import sys

def main():

	print("Calculating Averages")
	infile_path = sys.argv[1] #Directory path and conf-name
	print("Input File Path: "+ infile_path)

	infile_prog = sys.argv[2] #Directory path and conf-name
	print("Program Name: "+  infile_prog)

	infile_name = sys.argv[3] #Directory path and conf-name
	print("Input File Name: "+ infile_name)	

	repeat_str = sys.argv[4]
	print("Number of Repeats: "+ repeat_str)

	outfile_path = sys.argv[5]
	print("Output File Path: "+  outfile_path)

	outfile = open(outfile_path+"/"+infile_name, 'w+')
	#outfile.write('benchmarks-T \t ,sb \t ,bb \t ,bb+pf \t ,lrp \t ,lrp+pf \n')

	runs=[1,2]
	head=''
	count=0

	all=[]

	repeat=int(repeat_str)

	repeat_arr=[i for i in range(repeat)]

	max_rows=0
	max_cols=0

	num_cols=6
	num_row=20 #in case of all threads

	mat = [[[0 for col in range(num_cols)]for row in range(num_row)] for x in range(repeat)]

	averages=[[0 for col in range(num_cols)]for row in range(num_row)]
	
	#assume all are in the same program.

	#vetical
	for r in repeat_arr:	

		in_path=infile_path+ "/"+ infile_prog +"-"+str(r+1)+"/"+infile_name
		#print(in_path)
		infile = open(in_path, 'r')

		count=0
		#each row
		for line in infile:

			#print(line)
			if count == 0:
				head=line
				#print("Header: " + head)
				head_fileds = [x.strip() for x in head.split(',')]
				head_count=0
				for hf in head_fileds:
					#print(str(hf)+",")
					head_count=(head_count+1)
				#print("Head Count: "+ str(head_count))
				max_cols=head_count #Same
				#count = count + 1
				#continue

			fields = [x.strip() for x in line.split(',')]
			#size is the number of columns
			f_count=0
			#each column
			for f in fields:
				#print(str(f)+" , ")
				mat[r][count][f_count]=f
				#if f_count != 0:					
				#	mat[r][count][f_count]=f
				#else:					
				#	mat[r-1][count][f_count]=f

				f_count=f_count+1
			max_cols=f_count

			#print("--\n")
		
			count=count + 1 #line count in each file. #1 is always the header.
			max_rows=count
		#end of #
		infile.close()
	# It is good practice to close the file at the end to free up resources	
	infile.close()
	#output.close()


	#for i in range(repeat):
	#	print("Running : "+str(repeat))
	#	for j in range(max_rows):			
	#		for k in range(max_cols):
	#			print (str(mat[i][j][k]) +"\t"),
	#		print("\n")
	#	print("------------------------------------------------------------------------------------------------------")	

	#check if max rows and columas are less than 
	for j in range(max_rows):			
		for k in range(max_cols):

			#If corner cases then
			if j==0 or k==0:
				averages[j][k]=mat[0][j][k] #Those are the names
				continue
  			sum=0
  			r_count=0
			for i in range(repeat):
				#print mat[i][j][k]
				sum += float(mat[i][j][k])
				r_count=(r_count+1)

			average_val=sum/r_count
			averages[j][k]=average_val

	#Caclculate averaes
	print("Average Values of "+str(infile_name))
	print("----------------------------------------Average Values-----------------------------------------------")
	for j in range(max_rows):			
		for k in range(max_cols):
			print( ('%14s' % str(averages[j][k])) +" \t"),

			outfile.write( ('%14s' %  str(averages[j][k]) ) + " \t")
		print('\n')
		outfile.write("\n")
		
	print("------------------------------------------------------------------------------------------------------")	
	#outfile.write("End");
	outfile.close()

main()
