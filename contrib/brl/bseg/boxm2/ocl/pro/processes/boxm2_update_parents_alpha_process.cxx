// This is brl/bseg/boxm2/ocl/pro/processes/boxm2_update_parents_alpha_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief  A process for updating parents alpha by the max prob of the children
//
// \author Vishal Jain
// \date Apr 23, 2013

#include <vcl_fstream.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/boxm2_data_base.h>
#include <boxm2/ocl/boxm2_ocl_util.h>
//brdb stuff
#include <brdb/brdb_value.h>

//directory utility
#include <vul/vul_timer.h>
#include <vcl_where_root_dir.h>
#include <bocl/bocl_device.h>
#include <bocl/bocl_kernel.h>


namespace boxm2_ocl_update_parents_alpha_process_globals
{
    const unsigned n_inputs_ = 3;
    const unsigned n_outputs_ = 0;
    void compile_kernel(bocl_device_sptr device, bocl_kernel* merge_kernel)
    {
        //gather all render sources... seems like a lot for rendering...
        vcl_vector<vcl_string> src_paths;
        vcl_string source_dir = boxm2_ocl_util::ocl_src_root();
        src_paths.push_back(source_dir + "scene_info.cl");
        src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
        src_paths.push_back(source_dir + "bit/update_parents_alpha.cl");

        merge_kernel->create_kernel( &device->context(),
                                     device->device_id(),
                                     src_paths,
                                     "update_parents_alpha",
                                     "",
                                     "boxm2 opencl update parents alpha"); //kernel identifier (for error checking)
    }

    //map of compiled kernels, organized by data type
    static vcl_map<vcl_string,bocl_kernel* > kernels;
}

bool boxm2_ocl_update_parents_alpha_process_cons(bprb_func_process& pro)
{
    using namespace boxm2_ocl_update_parents_alpha_process_globals;

    //process takes 1 input
    vcl_vector<vcl_string> input_types_(n_inputs_);
    input_types_[0] = "bocl_device_sptr";
    input_types_[1] = "boxm2_scene_sptr";
    input_types_[2] = "boxm2_opencl_cache_sptr";

    // process has 1 output:
    // output[0]: scene sptr
    vcl_vector<vcl_string>  output_types_(n_outputs_);
    return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}

bool boxm2_ocl_update_parents_alpha_process(bprb_func_process& pro)
{
    using namespace boxm2_ocl_update_parents_alpha_process_globals;
    if ( pro.n_inputs() < n_inputs_ ) {
        vcl_cout << pro.name() << ": The input number should be " << n_inputs_<< vcl_endl;
        return false;
    }
    float transfer_time=0.0f;
    float gpu_time=0.0f;
    //get the inputs
    unsigned i = 0;
    bocl_device_sptr device= pro.get_input<bocl_device_sptr>(i++);
    boxm2_scene_sptr scene =pro.get_input<boxm2_scene_sptr>(i++);
    boxm2_opencl_cache_sptr opencl_cache= pro.get_input<boxm2_opencl_cache_sptr>(i++);

    vcl_string identifier=device->device_identifier();
    // create a command queue.
    int status=0;
    cl_command_queue queue = clCreateCommandQueue(device->context(),
                                                  *(device->device_id()),
                                                  CL_QUEUE_PROFILING_ENABLE,&status);
    if (status!=0) {
        vcl_cout<<" ERROR in initializing a queue"<<vcl_endl;
        return false;
    }

    //set tree identifier and compile (indexes into merge tree kernel)
    if (kernels.find(identifier)==kernels.end())
    {
        vcl_cout<<"===========Compiling kernels==========="<<vcl_endl;
        bocl_kernel * kernel=new bocl_kernel();
        compile_kernel(device,kernel);
        kernels[identifier]=kernel;
    }

    float output_arr[100];
    for (int i=0; i<100; ++i) output_arr[i] = 0.0f;
    bocl_mem_sptr  cl_output=new bocl_mem(device->context(), output_arr, sizeof(float)*100, "output buffer");
    cl_output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    // bit lookup buffer
    cl_uchar lookup_arr[256];
    boxm2_ocl_util::set_bit_lookup(lookup_arr);
    bocl_mem_sptr lookup=new bocl_mem(device->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
    lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

    //2. set workgroup size
    opencl_cache->clear_cache();
    vcl_map<boxm2_block_id, boxm2_block_metadata> blocks = scene->blocks();
    vcl_map<boxm2_block_id, boxm2_block_metadata>::iterator blk_iter;
        bocl_kernel* kern=kernels[identifier];
    for (blk_iter = blocks.begin(); blk_iter != blocks.end(); ++blk_iter)
    {
        boxm2_block_id id = blk_iter->first;
        vcl_cout<<"Updating Parents Alpha"<<id<<vcl_endl;
        //clear cache
        boxm2_block_metadata data = blk_iter->second;
        int numTrees = data.sub_block_num_.x() * data.sub_block_num_.y() * data.sub_block_num_.z();

        //write the image values to the buffer
        vul_timer transfer;
        bocl_mem* blk       = opencl_cache->get_block(id);
        bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(id);
        bocl_mem* blk_info  = opencl_cache->loaded_block_info();
        transfer_time += (float) transfer.all();
        vcl_size_t lThreads[] = {16, 1};
        vcl_size_t gThreads[] = {RoundUp(numTrees,lThreads[0]), 1};

        //set first kernel args
        kern->set_arg( blk_info );
        kern->set_arg( lookup.ptr() );
        kern->set_arg( blk );
        kern->set_arg( alpha );
        kern->set_local_arg(lThreads[0]*lThreads[1]*16*sizeof(cl_uchar)  );
        kern->set_local_arg(lThreads[0]*lThreads[1]*10*sizeof(cl_uchar)  );
        //kern->set_arg( cl_output.ptr() );

        //execute kernel
        kern->execute( queue, 2, lThreads, gThreads);
        clFinish(queue);
        gpu_time += kern->exec_time();

        //clear render kernel args so it can reset em on next execution
        kern->clear_args();
        alpha->read_to_buffer(queue);
        clFinish(queue);

    }

    vcl_cout<<"Update Parents Alpha: "<<gpu_time<<vcl_endl;
    return true;
}

