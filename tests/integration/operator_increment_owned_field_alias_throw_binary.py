"""Check throwing operators through borrowed aliases across a binary boundary."""
import sys

import operator_increment_owned_field_throw_binary as common


common.RECEIVER = 'alias'
common.CONSUMER = common.CONSUMER.replace(
    'raii Holder holder = new Holder();',
    'raii Holder holder = new Holder(); borrowed Holder alias = holder;')


if __name__ == '__main__':
    sys.exit(common.main())
