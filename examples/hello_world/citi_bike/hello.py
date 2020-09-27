# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

from maro.simulator import Env
from maro.simulator.scenarios.citi_bike.common import Action, DecisionEvent

auto_event_mode = False
start_tick = 0
durations = 100
max_ep = 2

env = Env(scenario="citi_bike", topology="toy.4s_4t", start_tick=start_tick,
          durations=durations, snapshot_resolution=60)

print(env.summary)

for ep in range(max_ep):
    metrics = None
    decision_evt: DecisionEvent = None
    is_done = False
    action = None

    while not is_done:
        metrics, decision_evt, is_done = env.step(action)

        if decision_evt is not None:  # it will be None at the end
            action = Action(decision_evt.station_idx, 0, 10)

            # print(decision_evt.action_scope)

    station_ss = env.snapshot_list['stations']
    shortage_states = station_ss[::'shortage']
    print("total shortage", shortage_states.sum())

    trips_states = station_ss[::'trip_requirement']
    print("total trip", trips_states.sum())

    cost_states = station_ss[::["extra_cost", "transfer_cost"]]

    print("total cost", cost_states.sum())

    matrix_ss = env.snapshot_list["matrices"]

    # since we may have different snapshot resolution,
    # so we should use frame_index to retrieve index in snapshots of current tick
    last_snapshot_index = env.frame_index

    # trip adj
    # NOTE: we have not clear the trip adj at each tick so it is an accumulative value,
    # then we can just query last snapshot to calc total trips
    trips_adj = matrix_ss[last_snapshot_index::'trips_adj']

    # trips_adj = trips_adj.reshape((-1, len(station_ss)))

    print("total trips from trips adj", trips_adj.sum())

    fulfillments = station_ss[::"fulfillment"]

    env.reset()
