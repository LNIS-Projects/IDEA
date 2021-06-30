CLK_PERIOD := 10
STA := sta
LSORACLE := lsoracle
ABC := yosys-abc
YOSYS := yosys
YOSYS_PLUGIN := oracle
TEE := cat - >
ISCAS85 := iscas85/c432.v iscas85/c880a.v iscas85/c3540a.v iscas85/c5288.v iscas85/c2670a.v iscas85/c5315a.v iscas85/c3540.v iscas85/c1908a.v iscas85/c7552.v iscas85/c499.v iscas85/c2670.v iscas85/c1908.v iscas85/c5315.v iscas85/c1335.v
ISCAS89 := iscas89/s1196.v iscas89/s1238.v iscas89/s15850.v
OPDB := opdb/chip_bridge.pickle.v opdb/fpu.pickle.v opdb/gng.pickle.v # opdb/sparc_core.pickle.v
ORIGINAL_FILES := picorv32/picorv32.v ibex/ibex.v ${OPDB} # ${ISCAS85} ${ISCAS89}

################################################################
######################## Targets ###############################
################################################################

AIG_FILES := $(patsubst %.v,%.aig,${ORIGINAL_FILES})
AIG_LOGS := $(patsubst %.v,%.aig.log,${ORIGINAL_FILES})

SDC_SCRIPTS := $(patsubst %.v,%_lsoracle.sdc,${ORIGINAL_FILES}) $(patsubst %.v,%_abc.sdc,${ORIGINAL_FILES}) $(patsubst %.v,%_unoptimized.sdc,${ORIGINAL_FILES})

SYNTH_BASE := $(patsubst %.v,%.rtl,${ORIGINAL_FILES})
SYNTH_BASE_LOGS := $(patsubst %.rtl,%.synth.log,${SYNTH_BASE})

LSORACLE_OUTPUTS := $(patsubst %.rtl,%_lsoracle.rtl,${SYNTH_BASE})
LSORACLE_SYNTH_LOG := $(patsubst %.rtl,%_lsoracle.synth.log,${SYNTH_BASE})
LSORACLE_TECHMAP_OUTPUTS := $(patsubst %.rtl,%_lsoracle.mapped.v,${SYNTH_BASE})
LSORACLE_SYNTH_REPORTS := $(patsubst %.rtl,%_lsoracle.synth_report,${SYNTH_BASE})
LSORACLE_TECHMAP_LOGS := $(patsubst %.rtl,%_lsoracle.tech.log,${SYNTH_BASE})
LSORACLE_TIMING_REPORT := $(patsubst %.rtl,%_lsoracle.timing,${SYNTH_BASE})
LSORACLE_TIMING_LOG := $(patsubst %.rtl,%_lsoracle.sta.log,${SYNTH_BASE})

ABC_OUTPUTS := $(patsubst %.rtl,%_abc.rtl,${SYNTH_BASE})
UNOPT_OUTPUTS := $(patsubst %.rtl,%_unoptimized.rtl,${SYNTH_BASE})
SYNTH_OUTPUTS := ${LSORACLE_OUTPUTS} ${UNOPT_OUTPUTS} ${ABC_OUTPUTS}
SYNTH_LOGS := $(patsubst %.rtl,%.synth.log,${LSORACLE_OUTPUTS}) $(patsubst %.rtl,%.synth.log,${ABC_OUTPUTS})

SYNTH_REPORTS := $(patsubst %.synth.log,%.synth_report,${SYNTH_LOGS})
TECHMAP_OUTPUTS := $(patsubst %.rtl,%.mapped.v,${SYNTH_OUTPUTS})
TECHMAP_LOGS := $(patsubst %.rtl,%.tech.log,${SYNTH_OUTPUTS})
TIMING := $(patsubst %.mapped.v,%.sta.log,${TECHMAP_OUTPUTS})
TIMING_REPORTS := $(patsubst %.sta.log,%.timing,${TIMING})
EVERYTHING_REPORTS := $(patsubst %.v,%.everything,${ORIGINAL_FILES})
REPORTS := $(patsubst %.v,%.report,${ORIGINAL_FILES})
################################################################
######################## Phony targets #########################
################################################################
all: everything.png

.PHONY: extract synth reports techmap reports timing everything_reports clean all reset

.PRECIOUS: ${SDC_SCRIPTS} ${AIG_FILES} ${AIG_LOGS} ${SYNTH_BASE} ${SYNTH_BASE_LOGS} ${SYNTH_OUTPUTS} ${SYNTH_LOGS} ${REPORTS} ${TECHMAP_OUTPUTS} ${TECHMAP_LOGS} ${TIMING} ${SYNTH_REPORTS} ${TIMING_REPORTS} ${EVERYTHING_REPORTS} ${REPORTS}

.DELETE_ON_ERROR:

extract: ${SYNTH_BASE}

synth: $(SYNTH_OUTPUTS)

techmap: $(TECHMAP_OUTPUTS)

reports: ${REPORTS}

timing: ${TIMING}

sdc_skel: $(patsubst %.v,%.sdc,$(wildcard iscas89/*.v))

reset:
	rm -f ${LSORACLE_OUTPUTS} ${LSORACLE_SYNTH_LOG} ${LSORACLE_TECHMAP_OUTPUTS} ${LSORACLE_SYNTH_REPORTS} ${LSORACLE_TECHMAP_LOGS} ${LSORACLE_TIMING_REPORT} ${LSORACLE_TIMING_LOG} ${REPORTS}

clean: clean_reports
	rm -f ${AIG_FILES} ${AIG_LOGS} ${SYNTH_BASE} ${SYNTH_BASE_LOGS} ${SYNTH_OUTPUTS} ${SYNTH_LOGS} ${TECHMAP_OUTPUTS} ${TIMING} ${SDC_SCRIPTS} ${TECHMAP_LOGS}

clean_reports:
	rm -f ${SYNTH_REPORTS} ${TIMING_REPORTS} ${REPORTS} ${EVERYTHING_REPORTS}  everything.tsv everything.png
################################################################
######################### SYNTHESIS ############################
################################################################

%.rtl %.synth.log &: %.v
	${YOSYS} -Q -p "read_verilog -sv $<; synth -auto-top -flatten -noabc; rename -top top; write_rtlil $*.rtl; stat" | ${TEE} $*.synth.log

# This works in limited circumstances due to flip-flops
%.aig %.aig.log &: %.rtl
	${YOSYS} -Q -p "read_rtlil $<; dffunmap; aigmap; write_aiger $*.aig" | ${TEE} $*.aig.log

%_lsoracle.rtl %_lsoracle.synth.log %_lsoracle.parts %_lsoracle.fixed: %.rtl
	$(eval TEMP=$(shell mktemp))
	echo "ps -a; partitioning --sap --size 2048 --epsilon 6 -c $$(pwd)/ff_.ini -i $*_lsoracle.fixed -o $*_lsoracle.parts; optimization --strategy 3 --fixed_initial; ps -m; crit_path_stats; ntk_stats " > ${TEMP}
	${YOSYS} -m ${YOSYS_PLUGIN} -Q -p "read_rtlil $<; lsoracle -lso_exe ${LSORACLE} -script ${TEMP}; techmap; opt -purge; stat; write_rtlil $*_lsoracle.rtl" | ${TEE} $*_lsoracle.synth.log
	rm ${TEMP}

# %_lsoracle.rtl %_lsoracle.synth.log %_lsoracle.parts %_lsoracle.fixed: %.rtl
# 	$(eval TEMP=$(shell mktemp))
# 	echo "ps -a; partitioning -c ../../lso-benchmarks/ff_km1_kKaHyPar_sea20.ini -i $*_lsoracle.fixed -o $*_lsoracle.parts; optimization ; ps -m; crit_path_stats; ntk_stats " > ${TEMP}
# 	${YOSYS} -m ${YOSYS_PLUGIN} -Q -p "read_rtlil $<; lsoracle -lso_exe ${LSORACLE} -script ${TEMP}; techmap; opt -purge; stat; write_rtlil $*_lsoracle.rtl" | ${TEE} $*_lsoracle.synth.log
# 	rm ${TEMP}

%_unoptimized.rtl: %.rtl
	cp $< $@

%.hpg: %.rtl
	$(eval TEMP=$(shell mktemp))
	echo "write_hypergraph $@" > ${TEMP}
	${YOSYS} -m ${YOSYS_PLUGIN} -Q -p "read_rtlil $<; lsoracle -lso_exe ${LSORACLE} -script ${TEMP}"
	rm ${TEMP}

%_abc.rtl %_abc.synth.log &: %.rtl
	${YOSYS} -Q -p "read_rtlil $<; abc -script techmapping/resyn2rs.abc; techmap; stat; write_rtlil $*_abc.rtl" | ${TEE} $*_abc.synth.log

%.tech_equiv.log: %_lsoracle.mapped.v %_unoptimized.mapped.v
	${YOSYS} -p "read_verilog ${word 2,$^}; rename -top gold; design -stash gold;\
		read_verilog $<; rename -top gate; design -stash gate; \
		design -copy-from gold -as gold gold; design -copy-from gate -as gate gate; \
		equiv_make -inames gold gate equiv; hierarchy -top equiv; equiv_simple; equiv_status -assert" | ${TEE} $@
%.equiv.log: %_lsoracle.rtl %.rtl
	${YOSYS} -p "read_rtlil ${word 2,$^}; rename -top gold; design -stash gold;\
		read_rtlil $<; rename -top gate; design -stash gate; \
		design -copy-from gold -as gold gold; design -copy-from gate -as gate gate; \
		equiv_make -inames gold gate equiv; hierarchy -top equiv; equiv_simple; equiv_status -assert" | ${TEE} $@

%.mapped.v %.tech.log &: %.rtl
	CIRCUIT_INPUT=$* ${YOSYS} -Q -c techmapping/synth.tcl | ${TEE} $*.tech.log

################################################################
########################### TIMING #############################
################################################################

# this is there to help generate these, but they need to be tweaked to get clocks.
%.sdc_skel: %.rtl
	echo "create_clock -name clk -period ${CLK_PERIOD} {clk_i}" > $@
	echo "set_input_delay -clock clk 0 {" >> $@
	${YOSYS} -Q -p  'read_rtlil $<; select i:* -list' | sed -e '/^top/!d' -e 's/top\///' >> $@
	echo "}" >> $@
	echo "set_output_delay -clock clk 0 {" >> $@
	${YOSYS} -Q -p  'read_rtlil $<; select o:* -list' | sed -e '/^top/!d' -e 's/top\///' >> $@
	echo "}" >> $@

%_lsoracle.sdc: %.sdc
	cp $< $@

%_abc.sdc: %.sdc
	cp $< $@

%_unoptimized.sdc: %.sdc
	cp $< $@

%.sta.log: %.mapped.v %.sdc
	VERILOG_INPUT=$< VERILOG_TOP=top SDC_FILE=$*.sdc ${STA} -exit techmapping/sta.tcl | ${TEE} $@

################################################################
######################### REPORTS ##############################
################################################################
# Dummy columns if previous doesn't exist.
previous:
	mkdir -p previous

previous/%_lsoracle.everything: | previous
	@ echo -n "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" > $@

%.timing: %.sta.log %.tech.log
	@ echo generating report $@
	@ sed -n 's/^Combinational\s*\([-.e0-9]*\)\s*\([-.e0-9]*\)\s*\([-.e0-9]*\)\s*\([-.e0-9]*\)\s*\([.0-9]*%\).*/internal_power\t\1\nswitching_power\t\2\nleakage_power\t\3\ntotal_power\t\4\npercent_power\t\5/p' $< > $@
	@ sed -n 's/\s*-\([0-9.]*\)\s *data arrival time/\1/p' $< | python3 -c 'import sys; print("arrival\t", max([float(i) for i in sys.stdin.readlines()]))' >> $@
	@ sed -n 's/worst slack \([0-9.]*\)/worst_slack\t\1/p' $< >> $@
	@ sed -e "/Area/!d" -e "s/.*Area = *\([0-9.]*\) .*/area\t\1/" $*.tech.log >> $@

%_lsoracle.synth_report: %_lsoracle.synth.log
	@ echo generating report $@
	@ sed -n 's/LSOracle: nodes: \([0-9]*\)/nodes\t\1/p; s/LSOracle: inputs: \([0-9]*\)/inputs\t\1/p;  s/LSOracle: outputs: \([0-9]*\)/outputs\t\1/p;  s/LSOracle: AIG level: \([0-9]*\)/aig_level\t\1/p; s/LSOracle: AIG nodes: \([0-9]*\)/aig_nodes\t\1/p; s/LSOracle.* Using \([0-9]*\) partitions/partitions\t\1/p; s/LSOracle: Final ntk size = \([0-9]*\) and depth = \([0-9]*\)/opt_nodes\t\1\
opt_level\t\2/p; s/LSOracle: \([0-9]*\) AIGs and \([0-9]*\) MIGs/aigs\t\1\
migs\t\2/p; s/LSOracle: Full Optimization: \([0-9]*\)ms/runtime\t\1/p; s/LSOracle: MAJ nodes: \([0-9]*\)/mig_nodes\t\1/p;  s/LSOracle: MIG level: \([0-9]*\)/mig_level\t\1/p;  s/LSOracle: MAJ nodes internally = \([0-9]*\)/maj_nodes_internal\t\1/p;  s/LSOracle: AND nodes internally = \([0-9]*\)/and_nodes_internal\t\1/p' < $< > $@

%_abc.synth_report: %_abc.synth.log
	@ echo generating report $@
	@ sed -n 's/ABC:.*and = *\([0-9]*\) *lev = *\([0-9*]\)/aig_nodes\t\1\naig_levels\t\2/p' < $< > $@

%.report: %_lsoracle.synth_report %_lsoracle.timing %_abc.synth_report %_abc.timing %_unoptimized.timing
	@ echo "circuit\t$*" > $@
	cat $^ >> $@

%.everything: %.report
	@ echo generating report $@
	@ cut -f2 < $< | tr '\n' '\t' > $@
	@ echo >> $@

everything.tsv: ${EVERYTHING_REPORTS}
	@ echo generating report $@
	@ echo -n "circuit\tunopt_nodes\tunopt_inputs\tunopt_outputs\tunopt_aig_nodes\tunopt_aig_levels\t" > $@
	@ echo -n "partitions\taigs\tmigs\tfinal_nodes\tfinal_levels\truntime\t" >> $@
	@ echo -n "optimized_nodes\toptimized_inputs\toptimized_outputs\t" >> $@
	@ echo -n "mig_nodes\tmig_levels\tmaj_nodes\tand_nodes\t" >> $@
	@ echo -n "internal_power\tswitching_power\tleakage_power\ttotal_power\tpercent_power\tarrival\tworst_slack\tarea\t" >> $@
	@ echo -n "abc_aig_nodes\tabc_aig_levels\t" >> $@
	@ echo -n "abc_internal_power\tabc_switching_power\tabc_leakage_power\tabc_total_power\tabc_percent_power\t" >> $@
	@ echo -n "abc_arrival\tabc_worst_slack\tabc_area\t" >> $@
	@ echo -n "unopt_internal_power\tunopt_switching_power\tunopt_leakage_power\tunopt_total_power\tunopt_percent_power\t" >> $@
	@ echo "unopt_arrival\tunopt_worst_slack\tunopt_area\t" >> $@
	cat $^ >> $@

everything.png everything.pdf: everything.tsv
	python3 plot_bars.py everything.tsv "Latest LSOracle"  previous/everything.tsv "Previous LSOracle" $@
