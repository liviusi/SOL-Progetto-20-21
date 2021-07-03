#!/bin/bash

if [ $# -eq 0 ]; then
	echo "No log file has been specified. Aborting."
	exit 1
fi


GREEN="\033[0;32m"
RESET_COLOR="\033[0m"
LOG_FILE=$1


# starting

echo -e "DATA ${GREEN}READ${RESET_COLOR} BY CLIENTS"
# count lines containing readFile
n_READFILE=$(grep "readFile" -c $LOG_FILE)
# count lines containing readNFiles
n_READNFILES=$(grep "readNFiles" -c $LOG_FILE)
# take the sum
# get strings containing readFile, get part after "->", remove trailing dots, sum the values
READFILE_BYTES=$(grep -E "readFile.*->" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g' | { sum=0; while read num; do ((sum+=num)); done; echo $sum; })
# get strings containing readNFiles, get part after "->", remove trailing dots, sum the values
READNFILES_BYTES=$(grep -E "readNFiles.*->" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g' | { sum=0; while read num; do ((sum+=num)); done; echo $sum; })
READ_BYTES=$((READFILE_BYTES+READNFILES_BYTES))
n_READS=$((n_READFILE+n_READNFILES))
echo -e "\tNumber of reading operations: ${n_READS}."
echo -e "\t\treadFile : ${n_READFILE}."
echo -e "\t\treadNFiles : ${n_READNFILES}."
echo -e "\tRead size : ${READ_BYTES} [BYTES]."
echo -e "\t\treadFile : ${READFILE_BYTES} [BYTES]."
echo -e "\t\treadNFiles : ${READNFILES_BYTES} [BYTES]."

# calculate means

if [ ${n_READS} -gt 0 ]; then
	MEAN_READ=$(echo "scale=3; ${READ_BYTES} / ${n_READS}" | bc -l)
	echo -e "\tMean : ${MEAN_READ} [BYTES]."
fi
if [ ${n_READFILE} -gt 0 ]; then
	MEAN_READFILES=$(echo "scale=3; ${READFILE_BYTES} / ${n_READFILE}" | bc -l)
	echo -e "\t\treadFile : ${MEAN_READFILES} [BYTES]."
fi
if [ ${n_READNFILES} -gt 0 ]; then
	MEAN_READNFILES=$(echo "scale=3; ${READNFILES_BYTES} / ${n_READNFILES}" | bc -l)
	echo -e "\t\treadNFiles : ${MEAN_READNFILES} [BYTES]."
fi

# the logic is the same as before

echo -e "DATA ${GREEN}WRITTEN${RESET_COLOR} BY CLIENTS"
n_WRITEFILE=$(grep "writeFile" -c $LOG_FILE)
n_APPENDTOFILE=$(grep "appendToFile" -c $LOG_FILE)
n_WRITES=$((n_WRITEFILE+n_APPENDTOFILE))
WRITEFILE_BYTES=$(grep -E "writeFile.*->" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g' | { sum=0; while read num; do ((sum+=num)); done; echo $sum; })
APPENDTOFILE_BYTES=$(grep -E "appendToFile.*->" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g' | { sum=0; while read num; do ((sum+=num)); done; echo $sum; })
WRITE_BYTES=$((WRITEFILE_BYTES+APPENDTOFILE_BYTES))
echo -e "\tNumber of writing operations: ${n_WRITES}."
echo -e "\t\twriteFile : ${n_WRITEFILE}."
echo -e "\t\tappendToFile : ${n_APPENDTOFILE}."
echo -e "\tWrite size : ${WRITE_BYTES}."
echo -e "\t\twriteFile : ${WRITEFILE_BYTES}."
echo -e "\t\tappendToFile : ${APPENDTOFILE_BYTES}."

if [ ${n_WRITES} -gt 0 ]; then
	MEAN_WRITE=$(echo "scale=3; ${WRITE_BYTES} / ${n_WRITES}" | bc -l)
	echo -e "\tMean : ${MEAN_WRITE} [BYTES]."
fi
if [ ${n_WRITEFILE} -gt 0 ]; then
	MEAN_WRITEFILE=$(echo "scale=3; ${WRITEFILE_BYTES} / ${n_WRITEFILE}" | bc -l)
	echo -e "\t\twriteFile : ${MEAN_WRITEFILE} [BYTES]."
fi
if [ ${n_APPENDTOFILE} -gt 0 ]; then
	MEAN_APPENDTOFILE=$(echo "scale=3; ${APPENDTOFILE_BYTES} / ${n_APPENDTOFILE}" | bc -l)
	echo -e "\t\tappendToFile : ${MEAN_APPENDTOFILE} [BYTES]."
fi

echo -e "${GREEN}OPERATION${RESET_COLOR} DATA"
n_LOCKFILE=$(grep " lockFile" -c $LOG_FILE) # space is needed, otherwise it would also match with unlockFile
n_UNLOCKFILE=$(grep "unlockFile" -c $LOG_FILE)
n_OPENFILECREATELOCK=$(grep -cE "openFile.* 3 : " $LOG_FILE)
n_OPENFILELOCK=$(grep -cE "openFile.* 2 : " $LOG_FILE)
n_OPENLOCK=$((n_OPENFILECREATELOCK+n_OPENFILELOCK))
n_CLOSEFILE=$(grep "closeFile" -c $LOG_FILE)
echo -e "\tNumber of lockFile : ${n_LOCKFILE}."
echo -e "\tNumber of unlockFile : ${n_UNLOCKFILE}."
echo -e "\tNumber of open and lock : ${n_OPENLOCK}."
echo -e "\tNumber of closeFile : ${n_CLOSEFILE}."


echo -e "${GREEN}REQUESTS${RESET_COLOR} HANDLED PER WORKER ID:"
# get all strings containing square brackets, count the times every single one of them is repeated, remove square brackets
REQUESTS=$(grep -oP '\[.*?\]' $LOG_FILE  | sort | uniq -c | sed -e 's/[ \t]*//' | sed 's/[][]//g')
while IFS= read -r line; do
	array=($line)
	echo -e "\tID ${array[1]} - ${array[0]} [REQUESTS]."
done <<< "$REQUESTS"

echo -e "${GREEN}STORAGE${RESET_COLOR} DATA"
# get current online clients, take only the number from the string, remove trailing dot, sort, take first element
MAXCLIENTS=$(grep "Current online clients : " $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g' | sort -g -r | head -1)
EVICTIONS=$(grep -E "Victims : [1-9+]" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g' | wc -l)
MAXSIZE_BYTES=$(grep "Maximum size" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g')
MAXSIZE_MBYTES=$(echo "scale=3; ${MAXSIZE_BYTES} * 0.000001" | bc -l)
MAXFILES=$(grep "Maximum file" $LOG_FILE | grep -oE '[^ ]+$' | sed -e 's/\.//g')
echo -e "\tMaximum online clients : ${MAXCLIENTS}."
echo -e "\tReplacement algorithm got triggered : ${EVICTIONS} time(s)."
echo -e "\tMaximum reached size : ${MAXSIZE_MBYTES} [MB]."
echo -e "\tMaximum files stored : ${MAXFILES}."