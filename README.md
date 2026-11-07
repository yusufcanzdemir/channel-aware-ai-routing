# Channel-Aware AI Routing

AI-based dynamic traffic routing for wireless mesh networks. The system monitors link-level channel metrics (SNR, delay, packet loss), learns channel behaviour from past measurements, and predicts link quality to steer traffic away from degrading links before packets are lost.

The approach is evaluated in **NS-3** against classical routing protocols (**AODV**, **OLSR**) under several channel degradation scenarios, followed by a feasibility analysis for running the model on real devices.

## Approach

```
link measurements → quality prediction model → link cost → Dijkstra → routing tables
```

A centralized controller periodically collects link statistics, predicts near-future link quality with a lightweight ML model, converts predictions into link costs and installs shortest-cost routes on the nodes.

## Scenarios

- Increasing interference in a region of the network
- Path loss / shadowing increase
- Node failure
- Traffic congestion
- Jamming

Metrics: packet delivery ratio, end-to-end delay, throughput, jitter, routing overhead and recovery time after degradation.

## Repository Structure

| Folder | Content |
| --- | --- |
| `ns3/` | NS-3 simulation (C++), linked into `ns-3/scratch/ai_routing` |
| `scripts/` | Setup and batch experiment runners |
| `analysis/` | Data processing and plotting (Python) |
| `ml/` | Model training |
| `results/figures/` | Final plots |
| `docs/` | Design decisions and progress log |

## Requirements

- Ubuntu 22.04
- NS-3.44
- Python 3 with numpy, pandas, scikit-learn, matplotlib, xgboost, torch

## Quick Start

```bash
./scripts/setup.sh
cd ~/proje/ns-allinone-3.44/ns-3.44
./ns3 configure --enable-examples --enable-tests
./ns3 build
./ns3 run "ai_routing --routing=aodv --seed=1"
```

## Status

Work in progress — course project, 2026.
