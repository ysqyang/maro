from .base import DataModelBase

from maro.backends.frame import node, NodeBase, NodeAttribute
from maro.backends.backend import AttributeType


@node("distribution")
class DistributionDataModel(DataModelBase):
    unit_price = NodeAttribute(AttributeType.Int)

    # origin is stock_levels, used to save product and its number, we have to split it.
    product_list = NodeAttribute(AttributeType.Int, 1, is_list=True)
    check_in_price = NodeAttribute(AttributeType.Int, 1, is_list=True)
    delay_order_penalty = NodeAttribute(AttributeType.Int, 1, is_list=True)

    def __init__(self):
        super(DistributionDataModel, self).__init__()
        # TODO: not sure about this.
        self._unit_price = 0

    def initialize(self, configs: dict):
        if configs is not None:
            self._unit_price = configs.get("unit_price", 0)

            self.reset()

    def reset(self):
        super(DistributionDataModel, self).reset()
        self.unit_price = self._unit_price