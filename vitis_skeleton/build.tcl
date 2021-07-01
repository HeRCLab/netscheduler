open_project -reset proj_netscheduler_skeleton
add_files network.c
set_top mynetwork
open_solution -reset solution1
set_part  {xcvu9p-flga2104-2-i}
create_clock -period "200MHz"
csynth_design
exit
