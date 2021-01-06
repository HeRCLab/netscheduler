# netscheduler: prototype minimal latency scheduling for MLPs and (eventually) LSTMs

## Dependencies:
- GLPK (GNU Linear Programming Kit)
\
Tested with version 4.65-2
\
To install:
```
sudo apt-get install libglpk40
```
- Graphviz
\
Tested with version 2.42.2-3build2
\
To install:
```
sudo apt-get install graphviz
```

## To use:
- Edit the following defines in netscheduler.h:
1. *MLP topology*: NUM_LAYERS, NUM_INPUTS, HIDDEN_LAYER_SIZE, NUM_OUTPUTS (note this will eventually be replaced with a more sensible way to specify networks)
2. *Resource constraints*: NUM_MULTIPLIERS, NUM_ADDERS
3. *Slack*: SLACK
4. *Maximum iteration interval*: MAX_II

- Build
```
make
```

- Run
```
schednet
```

- Open the downstream files:
1. my_dag.pdf
2. network.c
