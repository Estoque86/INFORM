#!/bin/bash
main=./waf

SCRIPTPATH=`pwd`
#sim="NDNSIM"

fws="Inform"
#cdp="fix"

runs=4
logDir=RESULTS
infoDir=infoSim
#net=`grep 'net=' $iniFile | awk '{print $3}' | awk -F '=' '{print $2}' | awk -F '}' '{print $1}'`

#net=grid
#T=GRID
#A=1
#plateau=0
C=0.001
M=101146
estM=500000
netImport=""
clPerc=50    		# Percentage of core node a client is attached to. 
eta=0.7      		# Eta parameter to calculate the Q-value
delta=0.1    		# Treshold indicating when the Exploitation phase must be stopped.
maxExplor=3		# Max num of chunks for the Exploration phase
maxExploit=30 		# Max num of chunks for the Exploitation phase
qTabLifetime=100	# qTab lifitime [s]


numClients=35           #approximately
#L=200
#simDuration=5000
req=50000000
#req=1500

#for n in Geant SmallWorld_Triu Random_Triu
for n in smallWorld random #Geant_Topo_Only #random
do
	echo $n
        if [ $n == "smallWorld" ] || [ $n == "random" ]; then
                netImport=Adjacency
        else
                netImport=Annotated
        fi

        for k in 0.8 1 1.2             #alpha
        do
                for z in 1           #lambda
                do
                        let "simDuration = $req / $numClients"
                        #simDuration=`expr $req/$z`
                        echo "Durata"
			echo $simDuration
                        echo "Durata per lambda"
			let "realReq = $simDuration * $z"
                        echo $realReq
                        #realReq=$simDuration*$z
                        #echo $realReq > $logDir/req_L\=${z}_A\=${k}

                        for i in `seq 0 $runs`
                        do
                                j=`expr $i + 1`

                                /usr/bin/time -f "\n%E \t elapsed real time \n%U \t total CPU seconds used (user mode) \n%S \t total CPU seconds used by the system on behalf of the process \n%M \t memory (max resident set size) [kBytes] \n%x \t exit status" -o ${infoDir}/Info_SIM\=${fws}_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}_R\=${i}.txt $main --run "scratch/ndn-inform --uniqueContents\=${M} --contentCatalogFib\=${estM} --cacheToCatalog\=${C} --lambda\=${z} --alpha\=${k} --clientPerc\=${clPerc} --eta\=${eta} --delta\=${delta} --maxChunkExplorPhase\=${maxExplor} --maxChunkExploitPhase\=${maxExploit} --qTabEntryLifetime\=${qTabLifetime} --networkType\=${n} --topologyImport\=${netImport} --simDuration\=${simDuration} --RngSeed\=1 --RngRun\=${j}" > $logDir/stdout/logSIM\=${fws}_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}_R\=${i}.out 2>&1

                        done

                        tar -zcvf /media/DATI/tortelli/COBRA/Simulatore/COBRA_SIM/ns\-\3/$logDir/postElab/compressedNAS/ResNewCobra/${fws}/log_S\=${fws}_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}.tar.gz $logDir/${fws}/DATA/Data*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/DATA/APP/Data*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/INTEREST/Interest*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/INTEREST/APP/Interest*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/DOWNLOAD/APP/Download*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}*

                        rm $logDir/${fws}/DATA/Data*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/DATA/APP/Data*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/INTEREST/Interest*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/INTEREST/APP/Interest*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}* $logDir/${fws}/DOWNLOAD/APP/Download*_N\=${n}_eta\=${eta}_delta\=${delta}_chunkExplor\=${maxExplor}_chunkExploit\=${maxExploit}_qTabLife\=${qTabLifetime}_A\=${k}*

                done
        done
done

## CALCOLO METRICHE
#paste -d\; ${logDir}/Interests_SIM_${sim}_S_${fws}_${cdp}_RUNS ${logDir}/Data_SIM_${sim}_S_${fws}_${cdp}_RUNS | awk -F\; '{print $1+$2}' OFS=\; > ${logDir}/LOAD_SIM_${sim}_S_${fws}_${cdp}_RUNS
#paste -d\; ${logDir}/Hits_SIM_${sim}_S_${fws}_${cdp}_RUNS ${logDir}/Misses_SIM_${sim}_S_${fws}_${cdp}_RUNS | awk -F\; '{print $1+$2}' OFS=\; > ${logDir}/Requests_SIM_${sim}_S_${fws}_${cdp}_RUNS
#paste -d\; ${logDir}/Hits_SIM_${sim}_S_${fws}_${cdp}_RUNS ${logDir}/Requests_SIM_${sim}_S_${fws}_${cdp}_RUNS | awk -F\; '{print $1/$2}' OFS=\; > ${logDir}/HIT_SIM_${sim}_S_${fws}_${cdp}_RUNS

#awk '{sum+=$1} END { print sum/NR}' ${logDir}/HIT_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${logDir}/HIT_SIM_${sim}_S_${fws}_${cdp}
#awk '{sum+=$1} END { print sum/NR}' ${logDir}/DISTANCE_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${logDir}/DISTANCE_SIM_${sim}_S_${fws}_${cdp}
#awk '{sum+=$1} END { print sum/NR}' ${logDir}/LOAD_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${logDir}/LOAD_SIM_${sim}_S_${fws}_${cdp}


#awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}' ${logDir}/HIT_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${logDir}/HIT_SIM_${sim}_S_${fws}_${cdp}
#awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}' ${logDir}/DISTANCE_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${logDir}/DISTANCE_SIM_${sim}_S_${fws}_${cdp}
#awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}' ${logDir}/LOAD_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${logDir}/LOAD_SIM_${sim}_S_${fws}_${cdp}

#tar -zcvf ${logDir}/Results_SIM_${sim}_S\=${fws}_${cdp}.tar.gz ${logDir}/*S_${fws}_${cdp}*
#rm ${logDir}/*S_${fws}_${cdp}*


## CALCOLO PERFORMANCE
#cat $infoDir/Info_SIM* | grep "elapsed" | awk '{print $1}' > $infoDir/Elapsed_SIM_${sim}_S_${fws}_${cdp}_RUNS
#cat $infoDir/Info_SIM* | grep "user" | awk '{print $1}' > $infoDir/CPU_SIM_${sim}_S_${fws}_${cdp}_RUNS
#cat $infoDir/Info_SIM* | grep "memory" | awk '{print $1}' > $infoDir/Memory_SIM_${sim}_S_${fws}_${cdp}_RUNS

#awk '{sum+=$1} END { print sum/NR}' ${infoDir}/CPU_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${infoDir}/CPU_SIM_${sim}_S_${fws}_${cdp}
#awk '{sum+=$1} END { print sum/NR}' ${infoDir}/Memory_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${infoDir}/Memory_SIM_${sim}_S_${fws}_${cdp}

#awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}' ${infoDir}/CPU_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${infoDir}/CPU_SIM_${sim}_S_${fws}_${cdp}
#awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}' ${infoDir}/Memory_SIM_${sim}_S_${fws}_${cdp}_RUNS >> ${infoDir}/Memory_SIM_${sim}_S_${fws}_${cdp}

#tar -zcvf $infoDir/Performance_SIM_${sim}_S\=${fws}_${cdp}.tar.gz $infoDir/*S_${fws}_${cdp}*
#rm ${infoDir}/*S_${fws}_${cdp}*

