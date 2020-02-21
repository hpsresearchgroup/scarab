### FILES:
* cacti_dram_template.cfg
* gen_power_intf_infiles.pl
* mcpat_template.xml
* power_intf.pl
* scarab.pm


# DOWNLOAD McPAT
The scarab power model relies on McPAT. We tested Scarab with McPAT v1.0, which
can be found here:
http://www.hpl.hp.com/research/mcpat/

# DOWNLOAD CACTI
The scarab power model relies on CACTI. We tested Scarab with CACTI 6.5, which
can be found here:
http://www.hpl.hp.com/research/cacti/

# RUNNING WITH DVFS:
Scarab supports DVFS, however changes must be made to both McPAT and CACTI to
support this.  We have supplied these changes in two patch files, mcpat.patch
and cacti.patch. To apply the patch, first download McPAT and CACTI (following
the directions above), then apply the patch using the following command.

* McPat:
> cd <mcpat_v1.0>
> patch -s -p2 < mcpat.patch
> make -j

* Cacti:
> cd <cacti65>
> patch -s -p2 < cacti.patch
> make -j
