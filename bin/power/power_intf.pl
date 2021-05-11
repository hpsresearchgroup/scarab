#  Copyright 2020 HPS/SAFARI Research Groups
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy of
#  this software and associated documentation files (the "Software"), to deal in
#  the Software without restriction, including without limitation the rights to
#  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
#  of the Software, and to permit persons to whom the Software is furnished to do
#  so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.

#!/usr/bin/perl
#  Copyright 2020 HPS/SAFARI
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

use warnings;
use strict;

# Scarab calls this script to get access to power numbers.
# See src/power_intf.{c,h} for details.

use FindBin qw($Bin); # find where this script is located

die "Usage: $0 <work dir> <get voltage and frequency>\n" unless @ARGV >= 1;

my $MCPAT_EXEC = shift;
my $CACTI_EXEC = shift;
my $dir = shift;
my $get_voltage_and_freq = shift;
my $debug = shift;
my $file_tag = shift;

sub parse_mcpat_output;
sub parse_cacti_dram_output;
sub get_params;
sub get_stats;
sub traverse_stats;

if (!(defined $file_tag)) {
  $file_tag = "";
}

my $model_results_filename = "$dir/$file_tag"."power_model_results.out";
my $mcpat_design_filename = "$dir/$file_tag"."design.out";

die "PARAMS file not found: $dir/$file_tag"."PARAMS.out\n" unless -r "$dir/$file_tag"."PARAMS.out" || -r "$dir/$file_tag"."PARAMS.out.gz";

my $rc = 0;

print("Running McPat...\n");
my $mcpat_design_opt = (-r $mcpat_design_filename) ? "-load_design" : "-dump_design";
$rc = system("cd $dir && $MCPAT_EXEC $mcpat_design_opt -infile $dir/$file_tag"."mcpat_infile.xml > $dir/$file_tag"."mcpat.out");
die "Error running McPAT: $MCPAT_EXEC $mcpat_design_opt -infile $dir/$file_tag"."mcpat_infile.xml > $dir/$file_tag"."mcpat.out\n" if $rc;

print("Running CACTI...\n");
$rc = system("cd $dir && $CACTI_EXEC -infile $dir/$file_tag"."cacti_infile.cfg > $dir/$file_tag"."cacti_dram.out");
die "Error running CACTI\n" if $rc;

print("Parsing scarab params...\n");
my %params = get_params($dir);

print("Parsing scarab stats...\n");
my %stats  = get_stats("$dir", 0, 1);

my %values;

print("Parsing McPAT output ($dir/$file_tag"."mcpat.out) and CACTI output ($dir/$file_tag"."cacti_dram.out)...\n");
parse_mcpat_output("$dir/$file_tag"."mcpat.out", \%values, $get_voltage_and_freq);
$values{"MEMORY"} = parse_cacti_dram_output("$dir/$file_tag"."cacti_dram.out", $get_voltage_and_freq);

open my $out, '>', $model_results_filename ||
    die "Could not open $model_results_filename\n";

print("Printing Power Results...\n");
for my $domain (keys %values) {
    for my $result (keys %{$values{$domain}}) {
        print $out "$domain\t$result\t";
        if(defined $values{$domain}{$result}) {
            print $out $values{$domain}{$result}."\n";
        } else {
            print $out "1.0\n";
        }
    }
}
print("Power Finished Successfully!\n");

sub parse_mcpat_output($$$)
{
    my $filename = shift;
    my $values = shift;
    my $get_voltage_and_freq = shift;
    open my $file, "<", $filename || die "Could not open $filename\n";

    my %map = (
      #"Frequency (MHz)" => "FREQUENCY",
      #"Voltage" => "VOLTAGE",
        "Total Leakage" => "STATIC",
        "Subthreshold Leakage" => "SUBTHR_LEAKAGE",
        "Gate Leakage" => "GATE_LEAKAGE",
        "Runtime Dynamic" => "DYNAMIC",
        "Peak Dynamic" => "PEAK_DYNAMIC"
    );

    if ($get_voltage_and_freq) {
      $map{"Frequency (MHz)"} = "FREQUENCY";
      $map{"Voltage"} = "VOLTAGE";
    }

    $values->{"CHIP"} = {};
    my $num_cores;
    while(<$file>) {
        my $line = $_;
        print("[parse_mcpat_output] Parsing McPat line $line") if ($debug);
        if($line =~ /Total Cores: (\d+)/) {
            $num_cores = $1;
            print("[parse_mcpat_output] Found: Total Cores = $1\n") if ($debug);
            last;
        }
        if($line =~ /^\s*(.*\S)\s*=\s*(\S+)/) {
            my $result = $1;
            my $value = $2;
            if (defined $map{$result}) {
                $values->{"CHIP"}{$map{$result}} = $value;
                print("[parse_mcpat_output] Found: $result = $value\n") if ($debug);
            }
        }
    }

    #make sure we found all of the values we need from McPAT before continuing
    for my $value (values %map) {
      if (!(defined $values->{"CHIP"}{$value})) {
        die "Error: Could not find all CHIP values in McPAT output: $value. Note: must apply patches to MCPAT and CACTI to get voltage and frequency (bin/power/mcpat.patch,cacti.patch).\n";
      }
    }

    # change frequency unit from MHz to Hz
    $values->{"CHIP"}{"FREQUENCY"} *= 1e6 if($get_voltage_and_freq);

    # for now
    $values->{"CHIP"}{"MIN_VOLTAGE"} = 0.65; # HACK for 32nm TODO: how do we make this better?

    for my $core (0..$num_cores - 1) {
        while (<$file>) { last if /Core:/; }
        $values->{"CORE_$core"} = {};
        while(<$file>) {
            my $line = $_;
            last if $line =~ /^\s*$/;
            if($line =~ /^\s*(.*\S)\s*=\s*(\S+)/) {
                my $result = $1;
                my $value = $2;
                if (defined $map{$result}) {
                    $values->{"CORE_$core"}{$map{$result}} = $value;
                }
            }
        }
        $values->{"CORE_$core"}{"FREQUENCY"} = $values->{"CHIP"}{"FREQUENCY"} if ($get_voltage_and_freq);
        $values->{"CORE_$core"}{"MIN_VOLTAGE"} = $values->{"CHIP"}{"MIN_VOLTAGE"};
        $values->{"CORE_$core"}{"VOLTAGE"} = $values->{"CHIP"}{"VOLTAGE"} if ($get_voltage_and_freq);
        $values->{"CORE_$core"}{"STATIC"} = $values->{"CORE_$core"}{"SUBTHR_LEAKAGE"} + $values->{"CORE_$core"}{"GATE_LEAKAGE"};
    }

    $values->{"UNCORE"} = {};
    $values->{"UNCORE"}{"FREQUENCY"} = $values->{"CHIP"}{"FREQUENCY"} if ($get_voltage_and_freq);
    $values->{"UNCORE"}{"MIN_VOLTAGE"} = $values->{"CHIP"}{"MIN_VOLTAGE"};
    $values->{"UNCORE"}{"VOLTAGE"} = $values->{"CHIP"}{"VOLTAGE"} if ($get_voltage_and_freq);
    for my $result ("STATIC", "SUBTHR_LEAKAGE", "GATE_LEAKAGE", "DYNAMIC", "PEAK_DYNAMIC") {
        $values->{"UNCORE"}{$result} = $values->{"CHIP"}{$result};
        for my $core (0..$num_cores - 1) {
            $values->{"UNCORE"}{$result} -= $values->{"CORE_$core"}{$result};
        }
    }

    delete $values->{"CHIP"};

    close($file);

    return \%values;
}

sub parse_cacti_dram_output($$)
{
    my $filename = shift;
    my $get_voltage_and_freq = shift;
    open my $file, "<", $filename || die "Could not open $filename\n";

    my %values;

    my %cacti_values = (
        "Precharge Energy (nJ)" => undef,
        "Activate Energy (nJ)" => undef,
        "Read Energy (nJ)" => undef,
        "Write Energy (nJ)" => undef,
        "Leakage Power Open Page (mW)" => undef, # assume open page policy
        "Leakage Power I/O (mW)" => undef,
        "Refresh power (mW)" => undef,
        #"Voltage (V)" => undef
    );

    if ($get_voltage_and_freq) {
        $cacti_values{"Voltage (V)"} = undef;
    }

    while(<$file>) {
        my $line = $_;
        print("[parse_cacti_dram_output] Parsing CACTI Line: $line") if ($debug);
        last if $line =~ /Cache height/;
        if($line =~ /^\s*(.*\S)\s*:\s*(\S+)/) {
            my $result = $1;
            my $value = $2;
            if (exists $cacti_values{$result}) {
                $cacti_values{$result} = $value;
                print("[parse_cacti_dram_output] Found: $result = $value\n") if ($debug);
            }
        }
    }

    for my $key (keys %cacti_values) {
        die "CACTI value for $key not found\n" unless defined $cacti_values{$key};
    }

    $values{"VOLTAGE"}     = $cacti_values{"Voltage (V)"}          if ($get_voltage_and_freq); 
    $values{"MIN_VOLTAGE"} = $values{"VOLTAGE"}                    if ($get_voltage_and_freq); # for now
    $values{"FREQUENCY"}   = $params{"POWER_INTF_REF_MEMORY_FREQ"} if ($get_voltage_and_freq);

    my $power_dram_precharge_stat = 0;
    my $power_dram_activate_stat  = 0;
    my $power_dram_read_stat      = 0;
    my $power_dram_write_stat     = 0;
    for my $core_id (0..$params{"NUM_CORES"}-1) {
      $power_dram_precharge_stat += $stats{"POWER_DRAM_PRECHARGE"}[$core_id];
      $power_dram_activate_stat  += $stats{"POWER_DRAM_ACTIVATE"}[$core_id];
      $power_dram_read_stat      += $stats{"POWER_DRAM_READ"}[$core_id];
      $power_dram_write_stat     += $stats{"POWER_DRAM_WRITE"}[$core_id];
    }

    $values{"DYNAMIC"} =
        $cacti_values{"Precharge Energy (nJ)"} * $power_dram_precharge_stat +
        $cacti_values{"Activate Energy (nJ)"}  * $power_dram_activate_stat  +
        $cacti_values{"Read Energy (nJ)"}      * $power_dram_read_stat      +
        $cacti_values{"Write Energy (nJ)"}     * $power_dram_write_stat;

    if ($debug) {
      print("[parse_cacti_dram_output] DRAM dynamic energy:\n");
      my $dynamic_energy   = $values{"DYNAMIC"};
      my $precharge_energy = $cacti_values{"Precharge Energy (nJ)"};
      my $activate_energy  = $cacti_values{"Activate Energy (nJ)"};
      my $read_energy      = $cacti_values{"Read Energy (nJ)"};
      my $write_energy     = $cacti_values{"Write Energy (nJ)"};

      print("$dynamic_energy nJ (DYNAMIC) =
         $precharge_energy (Precharge Energy nJ) * $power_dram_precharge_stat (POWER_DRAM_PRECHARGE)+
         $activate_energy  (Activate Energy nJ)  * $power_dram_activate_stat  (POWER_DRAM_ACTIVATE) +
         $read_energy      (Read Energy nJ)      * $power_dram_read_stat      (POWER_DRAM_READ) +
         $write_energy     (Write Energy nJ)     * $power_dram_write_stat     (POWER_DRAM_WRITE)\n");
    }

    $values{"DYNAMIC"} *= 1.0e-9; # convert from nJoules to Joules

    my $time = $stats{"POWER_CYCLE"}[0]/$params{"POWER_INTF_REF_CHIP_FREQ"};
    if($debug) {
      print("[parse_cacti_dram_output] time:\n");
      my $num_cycles = $stats{"POWER_CYCLE"}[0];
      my $core_freq  = $params{"POWER_INTF_REF_CHIP_FREQ"};

      print("$time sec = $num_cycles (cycles) / $core_freq (freq Hz)\n");
    }

    if ($debug) {
      print("[parse_cacti_dram_output] DRAM dynamic power:\n");
      my $dynamic_energy = $values{"DYNAMIC"};
      my $dynamic_power  = $values{"DYNAMIC"} / $time; # convert to power (Watts)

      print("$dynamic_power Watts (DYNAMIC) = $dynamic_energy J / $time sec\n");
    }

    $values{"DYNAMIC"} /= $time; # convert to power (Watts)

    $values{"STATIC"} =
        $cacti_values{"Leakage Power Open Page (mW)"} +
        $cacti_values{"Leakage Power I/O (mW)"} +
        $cacti_values{"Refresh power (mW)"};

    if ($debug) {
      print("[parse_cacti_dram_output] DRAM static power:\n");
      my $total_leakage     = $values{"STATIC"};
      my $open_page_leakage = $cacti_values{"Leakage Power Open Page (mW)"};
      my $io_leakage        = $cacti_values{"Leakage Power I/O (mW)"};
      my $refresh_leakage   = $cacti_values{"Refresh power (mW)"};

      print("$total_leakage mW (STATIC) = $open_page_leakage (Leakage Power Open Page mW) + $io_leakage (Leakage Power I/O mW) + $refresh_leakage (Refresh power mW)");
    }

    $values{"STATIC"} *= 1.0e-3; # convert to Watts

    return \%values;
}

###############################################################
#From scarab.pm
###############################################################

# Return a hash (name -> value) for the parameters of the scarab run
# in the provided file
sub get_params($@) {
    my $dir = shift;
    my @filter_array = @_;
    my %filter_hash;
    for my $filter(@filter_array) {
        $filter_hash{$filter} = 1;
    }

    my $cmd = "";
    for my $file (`ls $dir/${file_tag}PARAMS.*{out,out.gz} 2> /dev/null`) {
        chomp $file;
        $cmd .= ($file =~ /gz$/ ? "zcat" : "cat")." $file ; ";
    }

    my %params;

    my $started = 0;

    for(`$cmd`) {
        next if !$started && $_ !~ /^Parameter/;
        $started = 1;

        if(/^(\S+)\s+(\([^"]*\)|\S+)(\s+(\S+))?\s*$/) {
            my $name = $1;
            die "Duplicate param: $name\n" if exists $params{$name};
            next if (scalar @filter_array) && !exists $filter_hash{$name};
            my $default_value = $2;
            my $specified_value = $4;

            # param type can be deduced from the default value
            if($default_value =~ /^(TRUE|FALSE|NULL)$/) {
                $default_value = ($default_value =~ /^TRUE$/);
            } elsif($default_value =~ /^\"(.*)\"$/) {
                $default_value = $1;
            } else {
                my $old = $default_value;
                $default_value = eval($default_value); # for default values defined as expressions
            }

            my $value = defined $specified_value ? $specified_value : $default_value;

            $params{$name} = $value;

            if(defined $params{$name}) {
              print("\t[get_params]: params{$name} = $params{$name}\n") if ($debug);
            }
        }
    }

    return %params;
}

# Traverse the stats in the specified directory and execute the given
# function with parameters name, value, core id

sub traverse_stats($$$$$) {
    my $dir = shift;
    my $core_id = shift; # undef means all cores
    my $include_warmup = shift;
    my $func = shift;
    my $power_intf = shift;

    my @files = $power_intf ?
        `ls $dir/${file_tag}*.stat.*{out,out.gz} 2> /dev/null` :
        `ls $dir/${file_tag}*.stat.*{out,out.gz} 2> /dev/null | grep -v scarab_power`;

    for my $file (@files) {
        chomp $file;
        next if $file =~ /ramulator/;
        die "Cannot parse core id from $file\n" unless $file =~ /stat\.(\d+)\.out/;
        my $file_core_id = $1;
        next if defined $core_id && $file_core_id != $core_id;
        my $cmd = ($file =~ /gz$/ ? "zcat" : "cat")." $file ; ";

        for(`$cmd`) {
            next if $_ =~ /^(\/|#|Core|Cumulative)/;
            my $name; my $value;
            if($_ =~ /^(\w+)\s+(\S+)/) {
                $name = $1;
                if ($include_warmup) {
                    if($_ =~ /^\w+\s+\S+\s+(\S+)\s*$/) {
                        $value = $1;
                    } elsif($_ =~ /^\w+\s+\S+\s+\S+\s+(\S+)\s+\S+\s*$/) {
                        $value = $1;
                    } else {
                        die "Could not parse stat $name\n";
                    }
                } else {
                    $value = $2;
                }
                $func->($name, $value, $file_core_id);
            }
        }
    }
}

# Return a hash (name -> value) for the stats of the scarab run in the
# provided directory

sub get_stats($$$@) {
    my $dir = shift;
    my $include_warmup = shift;
    my $power_intf = shift;
    my @filter_array = @_;
    my %filter_hash;
    for my $filter(@filter_array) {
        $filter_hash{$filter} = 1;
    }

    my %stats;

    traverse_stats($dir, undef, $include_warmup,
                   sub {
                       my ($name, $value, $core_id) = @_;
                       return if (scalar @filter_array) && !exists $filter_hash{$name};
                       $stats{$name} = [] if !exists $stats{$name};
                       die "Duplicate stat: $name\n" if exists $stats{$name}[$core_id];
                       $stats{$name}[$core_id] = $value;
                       print("\t[get_stats]: stats{$name}[$core_id] = $stats{$name}[$core_id]\n") if ($debug);
                   }, $power_intf);
    return %stats;
}
