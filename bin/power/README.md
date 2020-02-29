# Scarab Power Model


### Scarab power model files
Important power model files can be found under two directories:

1. ```<scarab_root>/bin/power/```  
2. ```<scarab_root>/src/power/```


## Download McPAT
The scarab power model relies on McPAT. Scarab has been tested and works with McPAT v1.0, which
can be found [here](http://www.hpl.hp.com/research/mcpat/).

## Download CACTI
The scarab power model relies on CACTI. Scarab has been tested and works with CACTI 6.5, which
can be found [here](http://www.hpl.hp.com/research/cacti/).

## Running with Dynamic Voltage-and-Frequency Scaling (DVFS):
Scarab supports DVFS, however, changes must be made to both McPAT and CACTI to
support this.

These changes are supplied in two patch files, mcpat.patch and cacti.patch. To apply the patches, first download McPAT and CACTI (following the directions above), then apply the patches using the following commands.

* McPat: 
 
>$ cd <mcpat_v1.0>  
>$ patch -s -p2 < <scarab\_root>/bin/power/mcpat.patch  
>$ make -j

* Cacti: 
 
>$ cd <cacti65>  
>$ patch -s -p2 < <scarab\_root>/bin/power/cacti.patch  
>$ make -j
