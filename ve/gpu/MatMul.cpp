/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/
#include <cassert>
#include <sstream>
#include <map>
#include <bh.h>
#include "cl.hpp"
#include "UserFuncArg.hpp"
#include "Kernel.hpp"
#include "Scalar.hpp"

namespace MatMul
{
    typedef std::map<OCLtype, Kernel> KernelMap;
    static KernelMap kernelMap;
    Kernel getKernel(const UserFuncArg* userFuncArg)
    {
        OCLtype dtype = (static_cast<BaseArray*>(userFuncArg->operands[0]))->type();
        KernelMap::iterator kit = kernelMap.find(dtype);
        if (kit == kernelMap.end())
        {
            std::stringstream source, kname;
            source << "#include <ocl_matmul.h>\n";
            source << "KERNEL(" << oclTypeStr(dtype) << ")\n";
            kname << "matmul_" << oclTypeStr(dtype);
            Kernel kernel(userFuncArg->resourceManager, 2, source.str(), kname.str());
            kernelMap.insert(std::make_pair(dtype, kernel));
            return kernel;
        } else {
            return kit->second;
        }
    }
}



extern "C" {
bh_error bh_matmul(bh_instruction *instr, void* ve_arg)
{
    
    bh_view *C = &instr->operand[0];
    bh_view *A = &instr->operand[1];
    bh_view *B = &instr->operand[2];
    assert (A->ndim == 2 && B->ndim == 2 && C->ndim == 2);
    bh_index ds2 = B->shape[0];
    bh_index ds1 = C->shape[0];
    bh_index ds0 = C->shape[1];
    assert (A->shape[0] == ds1 && A->shape[1] == ds2 && B->shape[1] == ds0);
    
    UserFuncArg* userFuncArg = (UserFuncArg*)ve_arg;
    Kernel kernel = MatMul::getKernel(userFuncArg);
    Kernel::Parameters kernelParameters;
    kernelParameters.push_back(std::make_pair(new Scalar(ds0), false));
    kernelParameters.push_back(std::make_pair(new Scalar(ds1), false));
    kernelParameters.push_back(std::make_pair(new Scalar(ds2), false));
    kernelParameters.push_back(std::make_pair(new Scalar(C->stride[0]),false));
    kernelParameters.push_back(std::make_pair(new Scalar(C->stride[1]),false));
    kernelParameters.push_back(std::make_pair(new Scalar(C->start),false));
    kernelParameters.push_back(std::make_pair(new Scalar(A->stride[0]),false));
    kernelParameters.push_back(std::make_pair(new Scalar(A->stride[1]),false));
    kernelParameters.push_back(std::make_pair(new Scalar(A->start),false));
    kernelParameters.push_back(std::make_pair(new Scalar(B->stride[0]),false));
    kernelParameters.push_back(std::make_pair(new Scalar(B->stride[1]),false));
    kernelParameters.push_back(std::make_pair(new Scalar(B->start),false));
    kernelParameters.push_back(std::make_pair(userFuncArg->operands[0],true));
    kernelParameters.push_back(std::make_pair(userFuncArg->operands[1],false));
    kernelParameters.push_back(std::make_pair(userFuncArg->operands[2],false));
    
    kernel.call(kernelParameters, {(size_t)C->shape[1],(size_t)C->shape[0]});
    return BH_SUCCESS;
}
}
