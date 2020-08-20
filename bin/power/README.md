# Scarab Power Model

This section first provides the steps to enable Scarab power model.  
The next section provides more detail on how Scarab power model works, which can be helpful if you need to modify components based on your research requirements.

## Enabling Scarab Power Model

### 1. Download McPAT
The scarab power model relies on McPAT. Scarab has been tested and works with McPAT v1.0, which
can be found [here](http://www.hpl.hp.com/research/mcpat/).

### 2. Download CACTI
The scarab power model relies on CACTI. Scarab has been tested and works with CACTI 6.5, which
can be found [here](http://www.hpl.hp.com/research/cacti/).

### 3. Environment Variables
Scarab uses ```MCPAT_BIN``` and ```CACTI_BIN``` to find McPat and CACTI binaries. You can use the following commands:
>$ export MCPAT\_BIN=<mcpat\_root\_dir>/mcpat  
>$ export CACTI\_BIN=<cacti\_root\_dir>/cacti

By default, Scarab looks for McPat and CACTI binaries in path specified by ```__mcpat_bin__``` and ```__cacti_bin__```, which are defined in ```<scarab_root>/bin/scarab_globals/scarab_paths.py```. Changing these variables is another way you can set the paths to the power tools binaries.

### 4. Power Simulation Parameter
```power_intf_on``` enables the power simulation. Similar to other Scarab parameters, this can be enabled in the parameters file or via command-line arguments when launching Scarab.

### 5. Enabling Dynamic Voltage-and-Frequency Scaling (DVFS):
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

## Scarab Power Model Organization
Important power model files can be found under two directories:

1. ```<scarab_root>/bin/power/```
2. ```<scarab_root>/src/power/```
