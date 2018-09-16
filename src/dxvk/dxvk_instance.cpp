#include <version.h>

#include "dxvk_instance.h"
#include "dxvk_openvr.h"

#include <algorithm>

namespace dxvk {
  
  DxvkInstance::DxvkInstance() {
    Logger::info(str::format("Game: ", env::getExeName()));
    Logger::info(str::format("DXVK: ", DXVK_VERSION));

    m_config = Config::getUserConfig();
    m_config.merge(Config::getAppConfig(env::getExeName()));
    m_config.logOptions();

    g_vrInstance.initInstanceExtensions();

    m_vkl = new vk::LibraryFn();
    m_vki = new vk::InstanceFn(this->createInstance());

    m_adapters = this->queryAdapters();
    g_vrInstance.initDeviceExtensions(this);
  }
  
  
  DxvkInstance::~DxvkInstance() {
    
  }
  
  
  Rc<DxvkAdapter> DxvkInstance::enumAdapters(uint32_t index) const {
    return index < m_adapters.size()
      ? m_adapters[index]
      : nullptr;
  }
  
  
  VkInstance DxvkInstance::createInstance() {
    DxvkInstanceExtensions insExtensions;

    std::array<DxvkExt*, 3> insExtensionList = {{
      &insExtensions.khrGetPhysicalDeviceProperties2,
      &insExtensions.khrSurface,
      &insExtensions.khrWin32Surface,
    }};

    DxvkNameSet extensionsEnabled;
    DxvkNameSet extensionsAvailable = DxvkNameSet::enumInstanceExtensions(m_vkl);
    
    if (!extensionsAvailable.enableExtensions(
          insExtensionList.size(),
          insExtensionList.data(),
          extensionsEnabled))
      throw DxvkError("DxvkInstance: Failed to create instance");
    
    // Enable additional extensions if necessary
    extensionsEnabled.merge(g_vrInstance.getInstanceExtensions());
    DxvkNameList extensionNameList = extensionsEnabled.toNameList();
    
    Logger::info("Enabled instance extensions:");
    this->logNameList(extensionNameList);
    
    VkApplicationInfo appInfo;
    appInfo.sType                 = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext                 = nullptr;
    appInfo.pApplicationName      = nullptr;
    appInfo.applicationVersion    = 0;
    appInfo.pEngineName           = "DXVK";
    appInfo.engineVersion         = VK_MAKE_VERSION(0, 7, 2);
    appInfo.apiVersion            = 0;
    
    VkInstanceCreateInfo info;
    info.sType                    = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext                    = nullptr;
    info.flags                    = 0;
    info.pApplicationInfo         = &appInfo;
    info.enabledLayerCount        = 0;
    info.ppEnabledLayerNames      = nullptr;
    info.enabledExtensionCount    = extensionNameList.count();
    info.ppEnabledExtensionNames  = extensionNameList.names();
    
    VkInstance result = VK_NULL_HANDLE;
    if (m_vkl->vkCreateInstance(&info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkInstance::createInstance: Failed to create Vulkan instance");
    return result;
  }
  
  
  std::vector<Rc<DxvkAdapter>> DxvkInstance::queryAdapters() {
    DxvkDeviceFilter filter;
    
    uint32_t numAdapters = 0;
    if (m_vki->vkEnumeratePhysicalDevices(m_vki->instance(), &numAdapters, nullptr) != VK_SUCCESS)
      throw DxvkError("DxvkInstance::enumAdapters: Failed to enumerate adapters");
    
    std::vector<VkPhysicalDevice> adapters(numAdapters);
    if (m_vki->vkEnumeratePhysicalDevices(m_vki->instance(), &numAdapters, adapters.data()) != VK_SUCCESS)
      throw DxvkError("DxvkInstance::enumAdapters: Failed to enumerate adapters");
    
    std::vector<Rc<DxvkAdapter>> result;
    for (uint32_t i = 0; i < numAdapters; i++) {
      Rc<DxvkAdapter> adapter = new DxvkAdapter(this, adapters[i]);
      
      if (filter.testAdapter(adapter))
        result.push_back(adapter);
    }

    const auto preferred_adapter = env::getEnvVar(L"DXVK_DEFAULT_ADAPTER");
    Logger::info(std::string("Default adapter=:") + preferred_adapter);
    
    std::sort(result.begin(), result.end(),
      [this, preferred_adapter] (const Rc<DxvkAdapter>& a, const Rc<DxvkAdapter>& b) -> bool {
      	const auto a_is_preferred = (preferred_adapter == a->deviceProperties().deviceName);
      	const auto b_is_preferred = (preferred_adapter == b->deviceProperties().deviceName);
      	
      	if (a_is_preferred == b_is_preferred) {
          return a->deviceProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && b->deviceProperties().deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        }
        
        return (a_is_preferred);
      });
    
    if (result.size() == 0) {
      Logger::warn("DXVK: No adapters found. Please check your "
                   "device filter settings and Vulkan setup.");
    }
    
    return result;
  }
  
  
  void DxvkInstance::logNameList(const DxvkNameList& names) {
    for (uint32_t i = 0; i < names.count(); i++)
      Logger::info(str::format("  ", names.name(i)));
  }
  
}
