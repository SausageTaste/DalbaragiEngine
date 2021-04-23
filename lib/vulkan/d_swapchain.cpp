#include "d_swapchain.h"

#include <array>

#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>

#include "d_logger.h"


namespace {

    template <typename T>
    constexpr T clamp(const T v, const T minv, const T maxv) {
        return std::max<T>(minv, std::min<T>(maxv, v));
    }


    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        for ( const auto& format : available_formats ) {
            if ( format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
                return format;
            }
        }

        return available_formats[0];
    }

    enum class PresentMode { fifo, mailbox };

    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes, const PresentMode preferred_mode) {
        for ( const auto& present_mode : available_present_modes ) {
            if (::PresentMode::fifo == preferred_mode && VK_PRESENT_MODE_FIFO_KHR == present_mode)
                return VK_PRESENT_MODE_FIFO_KHR;

            if (::PresentMode::mailbox == preferred_mode && VK_PRESENT_MODE_MAILBOX_KHR == present_mode)
                return VK_PRESENT_MODE_MAILBOX_KHR;
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, const unsigned w, const unsigned h) {
        if ( capabilities.currentExtent.width != UINT32_MAX ) {
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D extent = { w, h };

            extent.width = ::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = ::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return extent;
        }
    }

    uint32_t choose_image_count(const dal::SwapChainSupportDetails& swapchain_support) {
        uint32_t image_count = swapchain_support.m_capabilities.minImageCount + 1;

        if (swapchain_support.m_capabilities.maxImageCount > 0 && image_count > swapchain_support.m_capabilities.maxImageCount) {
            image_count = swapchain_support.m_capabilities.maxImageCount;
        }

        return image_count;
    }

    VkExtent2D get_identity_screen_resoultion(const VkSurfaceCapabilitiesKHR& capabilities) {
        uint32_t width = capabilities.currentExtent.width;
        uint32_t height = capabilities.currentExtent.height;
        if (
            capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
            capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR
        ) {
            return VkExtent2D{ height, width };
        }
        else {
            return VkExtent2D{ width, height };
        }
    }

}


// QueueFamilyIndices, SwapChainSupportDetails
namespace dal {

    void QueueFamilyIndices::init(const VkSurfaceKHR surface, const VkPhysicalDevice phys_device) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            const auto& queue_family = queue_families[i];

            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                this->m_graphics_family = i;

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, surface, &present_support);
            if (present_support)
                this->m_present_family = i;

            if (this->is_complete())
                break;
        }
    }

    bool QueueFamilyIndices::is_complete(void) const noexcept {
        if (this->NULL_VAL == this->m_graphics_family)
            return false;
        if (this->NULL_VAL == this->m_present_family)
            return false;

        return true;
    }


    void SwapChainSupportDetails::init(const VkSurfaceKHR surface, const VkPhysicalDevice phys_device) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &this->m_capabilities);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count, nullptr);
        if (0 != format_count) {
            m_formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count, m_formats.data());
        }

        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &present_mode_count, nullptr);
        if (0 != present_mode_count) {
            m_present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &present_mode_count, m_present_modes.data());
        }
    }

}


// SwapchainSyncManager
namespace dal {

    void SwapchainSyncManager::init(const uint32_t swapchain_count, const VkDevice logi_device) {
        this->destroy(logi_device);

        this->m_img_available.resize(MAX_FRAMES_IN_FLIGHT);
        for (auto& sem : this->m_img_available) {
            sem.init(logi_device);
        }

        this->m_render_finished.resize(MAX_FRAMES_IN_FLIGHT);
        for (auto& sem : this->m_render_finished) {
            sem.init(logi_device);
        }

        this->m_frame_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
        for (auto& fence : this->m_frame_in_flight_fences) {
            fence.init(logi_device);
        }

        this->m_img_in_flight_fences.resize(swapchain_count, nullptr);
    }

    void SwapchainSyncManager::destroy(const VkDevice logi_device) {
        for (auto& sem : this->m_img_available) {
            sem.destory(logi_device);
        }
        this->m_img_available.clear();

        for (auto& sem : this->m_render_finished) {
            sem.destory(logi_device);
        }
        this->m_render_finished.clear();

        for (auto& x : this->m_frame_in_flight_fences) {
            x.destory(logi_device);
        }
        this->m_frame_in_flight_fences.clear();

        this->m_img_in_flight_fences.clear();
    }

}


// SwapchainSpec
namespace dal {

    bool SwapchainSpec::operator==(const SwapchainSpec& other) const {
        if (this->m_extent != other.m_extent)
            return false;
        if (this->m_image_format != other.m_image_format)
            return false;
        if (this->m_count != other.m_count)
            return false;

        return true;
    }

    void SwapchainSpec::set(const uint32_t count, const VkFormat format, const VkExtent2D extent) {
        this->m_image_format = format;
        this->m_count = count;

        this->m_extent.x = extent.width;
        this->m_extent.y = extent.height;
    }

}


// SwapchainManager
namespace dal {

    SwapchainManager::~SwapchainManager() {
        dalAssert(VK_NULL_HANDLE == this->m_swapChain);
    }

    void SwapchainManager::init(
        const unsigned desired_width,
        const unsigned desired_height,
        const QueueFamilyIndices& indices,
        const VkSurfaceKHR surface,
        const VkPhysicalDevice phys_device,
        const VkDevice logi_device
    ) {
        this->destroy_except_swapchain(logi_device);

        const SwapChainSupportDetails swapchain_support{ surface, phys_device };
        const auto surface_format = ::choose_surface_format(swapchain_support.m_formats);
        const auto present_mode = ::choose_present_mode(swapchain_support.m_present_modes, ::PresentMode::fifo);

        this->m_image_format = surface_format.format;
        //this->m_extent = ::choose_extent(swapchain_support.m_capabilities, desired_width, desired_height);
        this->m_identity_extent = ::get_identity_screen_resoultion(swapchain_support.m_capabilities);
        this->m_transform = swapchain_support.m_capabilities.currentTransform;

        const auto needed_images_count = ::choose_image_count(swapchain_support);

        // Create swap chain
        {
            VkSwapchainCreateInfoKHR create_info_swapchain{};
            create_info_swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            create_info_swapchain.surface = surface;
            create_info_swapchain.minImageCount = needed_images_count;
            create_info_swapchain.imageFormat = surface_format.format;
            create_info_swapchain.imageColorSpace = surface_format.colorSpace;
            create_info_swapchain.imageExtent = this->m_identity_extent;
            create_info_swapchain.imageArrayLayers = 1;
            create_info_swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            const std::array<uint32_t, 2> queue_family_indices{ indices.graphics_family(), indices.present_family() };
            if (indices.graphics_family() != indices.present_family()) {
                create_info_swapchain.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                create_info_swapchain.queueFamilyIndexCount = queue_family_indices.size();
                create_info_swapchain.pQueueFamilyIndices = queue_family_indices.data();
                dalWarn("Graphics queue and present qeueu is not same");
            }
            else {
                create_info_swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            create_info_swapchain.preTransform = swapchain_support.m_capabilities.currentTransform;
            create_info_swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            create_info_swapchain.presentMode = present_mode;
            create_info_swapchain.clipped = VK_TRUE;
            create_info_swapchain.oldSwapchain = this->m_swapChain;

            const auto create_result_swapchain = vkCreateSwapchainKHR(logi_device, &create_info_swapchain, nullptr, &this->m_swapChain);
            dalAssert(VK_SUCCESS == create_result_swapchain);
        }

        // Create images
        {
            uint32_t image_count = 0;
            vkGetSwapchainImagesKHR(logi_device, this->m_swapChain, &image_count, nullptr);
            this->m_images.resize(image_count, VK_NULL_HANDLE);
            vkGetSwapchainImagesKHR(logi_device, this->m_swapChain, &image_count, this->m_images.data());
        }

        // Image Views
        {
            this->m_views.resize(this->m_images.size());
            for ( size_t i = 0; i < this->m_images.size(); i++ ) {
                const auto result = this->m_views[i].init(
                    this->m_images[i],
                    this->m_image_format,
                    1,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    logi_device
                );
                dalAssert(result);
            }
        }

        // Transform values
        {
            switch (this->m_transform) {
                case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
                    this->m_pre_rotate_mat = glm::mat4{1};
                    this->m_perspective_ratio = static_cast<float>(this->width()) / static_cast<float>(this->height());
                    break;
                case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
                    this->m_pre_rotate_mat = glm::rotate(glm::mat4{1}, glm::radians<float>(90), glm::vec3{0, 0, 1});
                    this->m_perspective_ratio = static_cast<float>(this->height()) / static_cast<float>(this->width());
                    break;
                case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
                    this->m_pre_rotate_mat = glm::rotate(glm::mat4{1}, glm::radians<float>(180), glm::vec3{0, 0, 1});
                    this->m_perspective_ratio = static_cast<float>(this->width()) / static_cast<float>(this->height());
                    break;
                case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
                    this->m_pre_rotate_mat = glm::rotate(glm::mat4{1}, glm::radians<float>(270), glm::vec3{0, 0, 1});
                    this->m_perspective_ratio = static_cast<float>(this->height()) / static_cast<float>(this->width());
                    break;
                default:
                    dalAbort("Unkown swapchain transform");
            }
        }

        this->m_sync_man.init(this->size(), logi_device);

        // Report result
        {
            auto msg = fmt::format("Swapchain created{{ res: {}x{}", this->extent().width, this->extent().height);

            switch (this->m_image_format) {
                case VK_FORMAT_B8G8R8A8_SRGB:
                    msg += ", format: rgba8 srgb";
                    break;
                case VK_FORMAT_R8G8B8A8_UNORM:
                    msg += ", format: rgba8 unorm";
                    break;
                default:
                    msg += fmt::format(", format: unknown({})", static_cast<int>(this->m_image_format));
                    break;
            }

            switch (present_mode) {
                case VK_PRESENT_MODE_FIFO_KHR:
                    msg += ", present_mode: fifo";
                    break;
                case VK_PRESENT_MODE_MAILBOX_KHR:
                    msg += ", present_mode: mailbox";
                    break;
                case VK_PRESENT_MODE_IMMEDIATE_KHR:
                    msg += ", present_mode: immediate";
                    break;
                default:
                    msg += fmt::format(", mode: unknown({})", static_cast<int>(present_mode));
                    break;
            }

            msg += " }";

            dalInfo(msg.c_str());
        }
    }

    void SwapchainManager::destroy(const VkDevice logi_device) {
        this->destroy_except_swapchain(logi_device);

        if (VK_NULL_HANDLE != this->m_swapChain) {
            vkDestroySwapchainKHR(logi_device, this->m_swapChain, nullptr);
            this->m_swapChain = VK_NULL_HANDLE;
        }
    }

    uint32_t SwapchainManager::size() const {
        dalAssert(this->views().size() == this->m_images.size());
        return this->views().size();
    }

    bool SwapchainManager::is_format_srgb() const {
        switch (this->format()) {
            case VK_FORMAT_R8G8B8A8_UNORM:
                return false;
            case VK_FORMAT_B8G8R8A8_SRGB:
                return true;
            default:
                dalAbort(fmt::format(
                    "Cannot determin if a format is srgb: {}",
                    static_cast<int>(this->format())
                ).c_str());
        }
    }

    SwapchainSpec SwapchainManager::make_spec() const {
        SwapchainSpec result;
        result.set(this->size(), this->format(), this->extent());
        return result;
    }

    std::pair<ImgAcquireResult, SwapchainIndex> SwapchainManager::acquire_next_img_index(const FrameInFlightIndex& cur_img_index, const VkDevice logi_device) const {
        uint32_t img_index;

        const auto result = vkAcquireNextImageKHR(
            logi_device,
            this->m_swapChain,
            UINT64_MAX,
            this->m_sync_man.semaphore_img_available(cur_img_index).get(),  // Signaled when the presentation engine is finished using the image
            VK_NULL_HANDLE,
            &img_index
        );

        switch (result) {
            case VK_ERROR_OUT_OF_DATE_KHR:
                return { ImgAcquireResult::out_of_date, SwapchainIndex::max_value() };
            case VK_SUBOPTIMAL_KHR:
                return { ImgAcquireResult::suboptimal, SwapchainIndex{img_index} };
            case VK_SUCCESS:
                return { ImgAcquireResult::success, SwapchainIndex{img_index} };
            default:
                return { ImgAcquireResult::fail, SwapchainIndex::max_value() };
        }
    }

    // Private

    void SwapchainManager::destroy_except_swapchain(const VkDevice logi_device) {
        this->m_sync_man.destroy(logi_device);
        this->m_images.clear();

        for (auto& view : this->m_views) {
            view.destroy(logi_device);
        }
        this->m_views.clear();
    }

}
