#arguments will be: runTests inputdir outputdir maxthreads
#for each case, the script will be printing a string first
#the string will show in the following format: InputFile=nomeDoFicheiro NumThreads=númeroDeTarefas
#output will be filtered by the script to show the execution time of tecnicofs for every case

inputdir=$1
outputdir=$2
maxthreads=$3
mkdir -p $outputdir
for file in $inputdir/*
do 
    inputFile=$file
    outputFile=${file%.txt}
    outputFile=$outputdir/${outputFile##*/}-$maxthreads.txt
    echo InputFile=$inputFile NumThreads=$maxthreads
    ./tecnicofs $inputFile $outputFile $maxthreads
done

#output filename = nomeFicheiroEntrada-númeroDeTarefas.txt