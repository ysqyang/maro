# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

from typing import List


class VirtualMachine:
    """VM object.

    The VM lifetime equals to the deletion tick - creation tick.
    For example:
        A VM's cpu utilization is {tick: cpu_utilization} = {0: 0.1, 1: 0.4, 2: 0.2}.
        In our scenario, we define vm's creation tick as 0, and deletion tick as 3.
        Its lifetime will be deletion tick - creation tick = 3 - 0 = 3

    Args:
        id (int): The VM id.
        cpu_cores_requirement (int): The amount of virtual cores requested by VM.
        memory_requirement (int): The memory requested by VM. The unit is (GBs).
        lifetime (int): The lifetime of the VM, that is, deletion tick - creation tick.
    """
    def __init__(self, id: int, cpu_cores_requirement: int, memory_requirement: int, lifetime: int):
        # VM Requirement parameters.
        self.id: int = id
        self.cpu_cores_requirement: int = cpu_cores_requirement
        self.memory_requirement: int = memory_requirement
        self.lifetime: int = lifetime
        # VM utilization list with VM cpu utilization(%) in corresponding tick.
        self._utilization_series: List[float] = []
        # The physical machine Id that the VM is assigned.
        self.pm_id: int = -1
        self._cpu_utilization: float = 0.0
        self.creation_tick: int = -1
        self.deletion_tick: int = -1

    @property
    def cpu_utilization(self) -> float:
        return self._cpu_utilization

    @cpu_utilization.setter
    def cpu_utilization(self, cpu_utilization: float):
        self._cpu_utilization = min(max(0, cpu_utilization), 100)

    def get_utilization(self, cur_tick: int) -> float:
        if cur_tick - self.creation_tick > len(self._utilization_series):
            raise Exception(f"The tick {cur_tick} is invalid for the VM {self.id}.")

        return self._utilization_series[cur_tick - self.creation_tick]

    def add_utilization(self, cpu_utilization: float):
        """VM CPU utilization list.

        In the cpu_readings_file, all CPU utilization of all VMs are sorted by the timestamp, indexed by the VM ID.
        At each tick, we only read the CPU utilization at specific timestamp (tick).
        Hence, this function is designed to append the CPU utilization to the end of the corresponding
        VM utilization series one by one.
        """
        # If cpu_utilization is smaller than 0, it means the missing data in the cpu readings file.
        # TODO: We use the last utilization, it could be further refined to use average or others.
        if cpu_utilization < 0.0:
            self._utilization_series.append(self._utilization_series[-1])
        else:
            self._utilization_series.append(cpu_utilization)

    def get_historical_utilization_series(self, cur_tick: int) -> List[float]:
        """"Only expose the CPU utilization series before the current tick."""
        return self._utilization_series[:cur_tick - self.creation_tick + 1]
