// Copyright © 2014, German Neuroinformatics Node (G-Node)
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under the terms of the BSD License. See
// LICENSE file in the root of the Project.
//
// Author: Christian Kellner <kellner@bio.lmu.de>

#include <h5x/NDSize.hpp>

#include <h5x/BaseHDF5.hpp>

#ifndef NIX_DATASPACE_H
#define NIX_DATASPACE_H

namespace h5x {

class DataSpace : public BaseHDF5 {
public:

    DataSpace() : BaseHDF5(H5S_ALL) { }
    DataSpace(hid_t space) : BaseHDF5(space) { }
    DataSpace(const DataSpace &other) : BaseHDF5(other) { }

    NDSize extent() const;

    static DataSpace create(const NDSize &dims, const NDSize &maxdims = {});
    static DataSpace create(const NDSize &dims, bool maxdims_unlimited);

};

} //h5x::

#endif
