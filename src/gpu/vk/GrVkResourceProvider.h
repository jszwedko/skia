/*
* Copyright 2016 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#ifndef GrVkResourceProvider_DEFINED
#define GrVkResourceProvider_DEFINED

#include "GrGpu.h"
#include "GrVkDescriptorPool.h"
#include "GrVkPipelineState.h"
#include "GrVkResource.h"
#include "GrVkUtil.h"
#include "SkTArray.h"
#include "SkTDynamicHash.h"
#include "SkTHash.h"
#include "SkTInternalLList.h"

#include "vk/GrVkDefines.h"

class GrPipeline;
class GrPrimitiveProcessor;
class GrTextureParams;
class GrVkCommandBuffer;
class GrVkGpu;
class GrVkPipeline;
class GrVkRenderPass;
class GrVkRenderTarget;
class GrVkSampler;

class GrVkResourceProvider {
public:
    GrVkResourceProvider(GrVkGpu* gpu);
    ~GrVkResourceProvider();

    // Set up any initial vk objects
    void init();

    GrVkPipeline* createPipeline(const GrPipeline& pipeline,
                                 const GrPrimitiveProcessor& primProc,
                                 VkPipelineShaderStageCreateInfo* shaderStageInfo,
                                 int shaderStageCount,
                                 GrPrimitiveType primitiveType,
                                 const GrVkRenderPass& renderPass,
                                 VkPipelineLayout layout);

    // Finds or creates a simple render pass that matches the target, increments the refcount,
    // and returns.
    const GrVkRenderPass* findOrCreateCompatibleRenderPass(const GrVkRenderTarget& target);

    GrVkCommandBuffer* createCommandBuffer();
    void checkCommandBuffers();

    // Finds or creates a compatible GrVkDescriptorPool for the requested type and count.
    // The refcount is incremented and a pointer returned.
    // TODO: Currently this will just create a descriptor pool without holding onto a ref itself
    //       so we currently do not reuse them. Rquires knowing if another draw is currently using
    //       the GrVkDescriptorPool, the ability to reset pools, and the ability to purge pools out
    //       of our cache of GrVkDescriptorPools.
    GrVkDescriptorPool* findOrCreateCompatibleDescriptorPool(VkDescriptorType type, uint32_t count);

    // Finds or creates a compatible GrVkSampler based on the GrTextureParams.
    // The refcount is incremented and a pointer returned.
    GrVkSampler* findOrCreateCompatibleSampler(const GrTextureParams&, uint32_t mipLevels);

    sk_sp<GrVkPipelineState> findOrCreateCompatiblePipelineState(const GrPipeline&,
                                                                 const GrPrimitiveProcessor&,
                                                                 GrPrimitiveType,
                                                                 const GrVkRenderPass& renderPass);

    // For all our GrVkPipelineState objects, we require a layout where the first set contains two
    // uniform buffers, one for the vertex shader and one for the fragment shader. Thus it is
    // possible for us to use a shadered descriptor pool to allocate all these similar descriptor
    // sets. The caller is responsible for reffing the outPool for as long as the returned
    // VkDescriptor set is in use.
    void getUniformDescriptorSet(VkDescriptorSet*, const GrVkDescriptorPool** outPool);

    // Returns the compatible VkDescriptorSetLayout to use for uniform buffers. The caller does not
    // own the VkDescriptorSetLayout and thus should not delete it. This function should be used
    // when the caller needs the layout to create a VkPipelineLayout.
    VkDescriptorSetLayout getUniDSLayout() const { return fUniformDescLayout; }

    // Destroy any cached resources. To be called before destroying the VkDevice.
    // The assumption is that all queues are idle and all command buffers are finished.
    // For resource tracing to work properly, this should be called after unrefing all other
    // resource usages.
    void destroyResources();

    // Abandon any cached resources. To be used when the context/VkDevice is lost.
    // For resource tracing to work properly, this should be called after unrefing all other
    // resource usages.
    void abandonResources();

private:

#ifdef SK_DEBUG
#define GR_PIPELINE_STATE_CACHE_STATS
#endif

    class PipelineStateCache : public ::SkNoncopyable {
    public:
        PipelineStateCache(GrVkGpu* gpu);
        ~PipelineStateCache();

        void abandon();
        void release();
        sk_sp<GrVkPipelineState> refPipelineState(const GrPipeline&,
                                                  const GrPrimitiveProcessor&,
                                                  GrPrimitiveType,
                                                  const GrVkRenderPass& renderPass);

    private:
        enum {
            // We may actually have kMaxEntries+1 PipelineStates in context because we create a new
            // PipelineState before evicting from the cache.
            kMaxEntries = 128,
        };

        struct Entry;

        void reset();

        int                         fCount;
        SkTHashTable<Entry*, const GrVkPipelineState::Desc&, Entry> fHashTable;
        SkTInternalLList<Entry> fLRUList;

        GrVkGpu*                    fGpu;

#ifdef GR_PIPELINE_STATE_CACHE_STATS
        int                         fTotalRequests;
        int                         fCacheMisses;
#endif
    };

    // Initialiaze the vkDescriptorSetLayout used for allocating new uniform buffer descritpor sets.
    void initUniformDescObjects();

    GrVkGpu* fGpu;

    // Central cache for creating pipelines
    VkPipelineCache fPipelineCache;

    // Array of RenderPasses that only have a single color attachment, optional stencil attachment,
    // optional resolve attachment, and only one subpass
    SkSTArray<4, GrVkRenderPass*> fSimpleRenderPasses;

    // Array of CommandBuffers that are currently in flight
    SkSTArray<4, GrVkCommandBuffer*> fActiveCommandBuffers;

    // Stores GrVkSampler objects that we've already created so we can reuse them across multiple
    // GrVkPipelineStates
    SkTDynamicHash<GrVkSampler, uint16_t> fSamplers;

    // Cache of GrVkPipelineStates
    PipelineStateCache* fPipelineStateCache;

    // Current pool to allocate uniform descriptor sets from
    const GrVkDescriptorPool* fUniformDescPool;
    VkDescriptorSetLayout  fUniformDescLayout;
    //Curent number of uniform descriptors allocated from the pool
    int                fCurrentUniformDescCount;
    int                fCurrMaxUniDescriptors;

    enum {
        kMaxUniformDescriptors = 1024,
        kNumUniformDescPerSet = 2,
        kStartNumUniformDescriptors = 16, // must be less than kMaxUniformDescriptors
    };
};

#endif
