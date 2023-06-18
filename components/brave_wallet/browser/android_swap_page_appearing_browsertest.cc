#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "brave/components/constants/brave_paths.h"
#include "base/path_service.h"
#include "brave/components/brave_search/browser/brave_search_fallback_host.h"
#include "components/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "base/strings/string_util.h"
#include "base/files/scoped_temp_dir.h"
//#include "chrome/browser/ui/browser.h"
//#include "base/files/file_path.h"
#include "components/google/core/common/google_switches.h"
#include "weblayer/browser/webui/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "base/memory/ref_counted_memory.h"
#include "content/public/common/url_constants.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/browser/webui/url_data_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "brave/components/brave_wallet/common/features.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "brave/chromium_src/chrome/test/base/testing_browser_process.h"
#include "brave/components/brave_wallet/browser/eth_allowance_manager.h"
#include "base/files/file_util.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "brave/components/l10n/common/localization_util.h"
#include "brave/components/brave_wallet_page/resources/grit/brave_wallet_swap_page_generated_map.h"
#include "out/android_Release_x86/gen/components/grit/brave_components_resources.h"
#include "brave/browser/ui/webui/brave_wallet/android/swap_page_ui.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/base/resource/resource_bundle.h"

#include "brave/browser/brave_wallet/json_rpc_service_factory.h"
#include "brave/browser/brave_wallet/keyring_service_factory.h"
#include "brave/browser/brave_wallet/tx_service_factory.h"
#include "brave/components/brave_wallet/browser/blockchain_registry.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_delegate.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_observer_base.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/json_rpc_service.h"
#include "brave/components/brave_wallet/browser/keyring_service.h"
#include "content/public/common/mhtml_generation_params.h"
#include "content/public/common/bindings_policy.h"
#include "brave/components/cosmetic_filters/browser/cosmetic_filters_resources.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "brave/components/brave_shields/browser/ad_block_service.h"

using namespace std::chrono_literals;

namespace brave_wallet {
namespace {

constexpr char kMnemonic1[] =
    "divide cruise upon flag harsh carbon filter merit once advice bright "
    "drive";
constexpr char kPasswordBrave[] = "brave";
 
void BindCosmeticFiltersResourcesOnTaskRunner (
  mojo::PendingReceiver<cosmetic_filters::mojom::CosmeticFiltersResources>
      receiver) {
  mojo::MakeSelfOwnedReceiver(
    std::make_unique<cosmetic_filters::CosmeticFiltersResources>(
        g_brave_browser_process->ad_block_service()),
    std::move(receiver));
}

void BindCosmeticFiltersResources(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<cosmetic_filters::mojom::CosmeticFiltersResources>
        receiver) {
  g_brave_browser_process->ad_block_service()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&BindCosmeticFiltersResourcesOnTaskRunner,
                                std::move(receiver)));
}

}

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
public:
  explicit TestWebUIControllerFactory(const std::string& source_name) 
    : source_name_(source_name) {}

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
        LOG(INFO) << "!!!! CreateWebUIControllerForURL url:" << url << " url.host_piece():" << url.host_piece() << " url.path_piece():" << url.path_piece();

    if (url.host_piece() == "wallet") {
      return std::make_unique<SwapPageUI>(web_ui, source_name_);
    }

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
LOG(INFO) << "!!!! GetWebUIType url:" << url << " url.host_piece()" << url.host_piece() << " url.path_piece():" << url.path_piece();                                        

    if (url.SchemeIs(content::kChromeUIScheme) && url.host_piece() == "wallet" && url.path_piece() == "/swap")
      return reinterpret_cast<content::WebUI::TypeID>(1);

    return content::WebUI::kNoWebUI; 
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
LOG(INFO) << "!!!! UseWebUIForURL url:" << url;                                        
    return url.SchemeIs(content::kChromeUIScheme) || url == "about:blank";
  }
private:
std::string source_name_;
};

class AndroidBrowserTestSwap : public PlatformBrowserTest {
public:
  AndroidBrowserTestSwap() {
    factory_ = std::make_unique<TestWebUIControllerFactory>("wallet");
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
  }

  void SetUp() override {
    LOG(INFO) << "SetUp_0";
    PlatformBrowserTest::SetUp();


    // LOG(INFO) << "SetUp finish";
    // const auto accounts = keyring_service_->GetAllAccountsSync();
    // EXPECT_GT(accounts->accounts.size(), 1u) << "Accounts count, must not be 0";
    // LOG(INFO) << "Accounts size:" << accounts->accounts.size();
  }

 void SetUpOnMainThread() override {
    LOG(INFO) << "SetUpOnMainThread_0";
    PlatformBrowserTest::SetUpOnMainThread();

      InitWallet();

 }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }




  PrefService* GetPrefs() { return profile_->GetPrefs(); }
  TestingPrefServiceSimple* GetLocalState() { return local_state_->Get(); }
 
  base::FilePath get_temp_path() const { return temp_dir_.GetPath(); }
  bool has_mhtml_callback_run() const { return has_mhtml_callback_run_; }
  int64_t file_size() const { return file_size_; }
  absl::optional<std::string> file_digest() const { return file_digest_; }
  void MHTMLGeneratedWithResult(base::OnceClosure quit_closure,
                                const content::MHTMLGenerationResult& result) {
    has_mhtml_callback_run_ = true;
    file_size_ = result.file_size;
    file_digest_ = result.file_digest;
    std::move(quit_closure).Run();
  }

  const std::string GenerateHtml(const std::string& output_fname, const bool& print_content = false) {
    base::RunLoop run_loop;
    base::FilePath fpath(get_temp_path());
    base::FilePath fname(FILE_PATH_LITERAL(output_fname));
    base::FilePath fullpath = fpath.Append(fname);
    GetActiveWebContents()->GenerateMHTMLWithResult(content::MHTMLGenerationParams(fullpath), 
                    base::BindOnce(&AndroidBrowserTestSwap::MHTMLGeneratedWithResult,
                          base::Unretained(this), run_loop.QuitClosure()));
    // Block until the MHTML is generated.
    run_loop.Run();
    EXPECT_TRUE(has_mhtml_callback_run()) << "Unexpected error generating MHTML file";
    EXPECT_GT(file_size(), 0) << "Generated html file size must not be 0";

    std::string fcontent;
    if(base::ReadFileToString(fullpath, &fcontent)) {
      if(print_content) {
        LOG(INFO) << "Html Content: " << fcontent;
      }
      EXPECT_FALSE(fcontent.empty()) << "Generated html file content, must not be empty";
      return fcontent;
    }

    return {};
  }

  const std::string GetConsoleMessages(const content::WebContentsConsoleObserver& console_observer) const {
    std::stringstream ss;
    for(auto const& msg : console_observer.messages()) {
      ss << msg.message << std::endl;
    }
    return ss.str();
  }

protected:

 class TestContentBrowserClient : public ChromeContentBrowserClient {
   public:
    TestContentBrowserClient() = default;
    TestContentBrowserClient(const TestContentBrowserClient&) = delete;
    TestContentBrowserClient& operator=(const TestContentBrowserClient&) =
        delete;
    ~TestContentBrowserClient() override = default;

    void RegisterBrowserInterfaceBindersForFrame(
        content::RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
      LOG(INFO) << "!!! RegisterBrowserInterfaceBindersForFrame";
      ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
          render_frame_host, map);
      content::RegisterWebUIControllerInterfaceBinder<brave_wallet::mojom::PageHandlerFactory,
                                                      SwapPageUI>(map);
      map->Add<cosmetic_filters::mojom::CosmeticFiltersResources>(
          base::BindRepeating(&BindCosmeticFiltersResources));

    }
  };
  TestContentBrowserClient test_content_browser_client_;

  void InitWallet() {
    LOG(INFO) << "InitWallet_10";
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    content::SetBrowserClientForTesting(&test_content_browser_client_);

    LOG(INFO) << "InitWallet_20";

   scoped_feature_list_.InitAndEnableFeature(
        brave_wallet::features::kNativeBraveWalletFeature);

/**/     TestingProfile::Builder builder;
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    builder.SetPrefService(std::move(prefs));
    LOG(INFO) << "InitWallet_30";
    profile_ = builder.Build();
    LOG(INFO) << "InitWallet_40";
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());
    LOG(INFO) << "InitWallet_50";
    keyring_service_ =
        brave_wallet::KeyringServiceFactory::GetServiceForContext(profile_.get());
    json_rpc_service_ =
        brave_wallet::JsonRpcServiceFactory::GetServiceForContext(profile_.get());
    json_rpc_service_->SetAPIRequestHelperForTesting(
        shared_url_loader_factory_);
    tx_service = brave_wallet::TxServiceFactory::GetServiceForContext(profile_.get());
    wallet_service_ = std::make_unique<brave_wallet::BraveWalletService>(
        shared_url_loader_factory_,
        brave_wallet::BraveWalletServiceDelegate::Create(profile_.get()), keyring_service_,
        json_rpc_service_, tx_service, GetPrefs(), GetLocalState());
    eth_allowance_manager_ = std::make_unique<brave_wallet::EthAllowanceManager>(
        json_rpc_service_, keyring_service_, GetPrefs());

    base::RunLoop run_loop;
    keyring_service_->RestoreWallet(kMnemonic1, kPasswordBrave, false,
        base::BindLambdaForTesting([&](bool success) {
          ASSERT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run(); 

 /*    base::FilePath pak_path_base;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path_base));
    base::FilePath brave_resources_pak_path = pak_path_base.AppendASCII("paks/brave_resources.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        brave_resources_pak_path, ui::kScaleFactorNone);

    base::FilePath components_resources_pak_path = pak_path_base.AppendASCII("paks/components_resources.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        components_resources_pak_path, ui::kScaleFactorNone);

    base::FilePath resources_pak_path = pak_path_base.AppendASCII("paks/resources.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pak_path, ui::kScaleFactorNone);

    LOG(INFO) << "!!! pak_path_base:" << pak_path_base << " brave_resources_pak_path:" << brave_resources_pak_path;
 */
    LOG(INFO) << "InitWallet_5";

     brave_wallet::SetDefaultEthereumWallet(
       TabModelList::models()[0]->GetProfile()->GetPrefs(),
       brave_wallet::mojom::DefaultWallet::BraveWallet);
    LOG(INFO) << "InitWallet_6";    
  }

  base::ScopedTempDir temp_dir_;
  bool has_mhtml_callback_run_;
  int64_t file_size_;
  absl::optional<std::string> file_digest_;

  std::unique_ptr<TestWebUIControllerFactory> factory_;  
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<brave_wallet::KeyringService> keyring_service_;
  raw_ptr<brave_wallet::JsonRpcService> json_rpc_service_;
  raw_ptr<brave_wallet::TxService> tx_service;
  std::unique_ptr<brave_wallet::BraveWalletService> wallet_service_;
  std::unique_ptr<brave_wallet::EthAllowanceManager> eth_allowance_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
 

};

IN_PROC_BROWSER_TEST_F(AndroidBrowserTestSwap, TestSwapPageAppearing) {
  LOG(INFO) << "Test Started";
  //const auto accounts = keyring_service_->GetAllAccountsSync();
  //for(const auto& account : accounts->accounts) {
    //LOG(INFO) << "Account coin: " << account->account_id->coin << " Addr: " << account->address;
  //}

  GURL url = GURL("chrome://wallet/swap");
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  auto* web_contents = GetActiveWebContents();

  content::WebContentsConsoleObserver console_observer(web_contents);
  //console_observer.SetPattern("*Error*");
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->GetOutermostWebContents()->Focus();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  EXPECT_TRUE(web_contents->GetLastCommittedURL() == url)
                    << "Expected URL " << url << " but observed "
                    << web_contents->GetLastCommittedURL();

/*
auto result = content::EvalJs(web_contents, R"(document.documentElement.innerHTML)",content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1);
LOG(INFO) << "!!!CNTNT: " << result.value.DebugString();
LOG(INFO) << "!!!Error: " << result.error;
*/


//  EXPECT_FALSE(console_observer.Wait()) << "Console must not contain any errors: " << GetConsoleMessages(console_observer);
  
  GenerateHtml("swap_page_content", false);

LOG(INFO) << "Console: \n" << GetConsoleMessages(console_observer);

}

/*
IN_PROC_BROWSER_TEST_F(AndroidBrowserTestSwap, TestSwapPageAppearing) {
//  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
//                                        GURL("brave://wallet/swap")));
GURL url = GURL("chrome://wallet/swap");

content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  auto* web_contents = GetActiveWebContents();

content::WebContentsConsoleObserver console_observer(web_contents);
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->GetOutermostWebContents()->Focus();
  WaitForLoadStop(web_contents);

std::string page_html = "";
// auto result = content::EvalJs(web_contents->GetPrimaryMainFrame(), R"(document.documentElement.innerHTML)",content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1);
// page_html = result.value.DebugString();
// LOG(INFO) << "!!!Error: " << result.error;

page_html = content::ExecuteScriptAndGetValue(web_contents->GetPrimaryMainFrame(), R"(document.documentElement.innerHTML)").DebugString();
//page_html = content::ExecuteScriptAndGetValue(web_contents->GetFocusedFrame(), "document.getElementsByTagName(\'html\')[0].innerHTML").DebugString();
//page_html = content::ExecuteScriptAndGetValue(web_contents->GetPrimaryMainFrame(), "console.log(\'ssssss\')").DebugString();
//  WaitForLoadStop(web_contents);


for(auto const& msg : console_observer.messages()) {
  LOG(INFO) << "!!!MSG:" << msg.message;
}

LOG(INFO)// << "URL:" << web_contents->GetLastCommittedURL() 
          << " page_html:" << page_html;

bool is_same_url = web_contents->GetLastCommittedURL() == url;
if (!is_same_url) {
    DLOG(WARNING) << "Expected URL " << url << " but observed "
                  << web_contents->GetLastCommittedURL();
  }
EXPECT_TRUE(is_same_url);

base::FilePath fpath(temp_dir_.GetPath());

auto filename(base::JoinString({"mypage", std::to_string(std::rand()), ".html"},"_"));
  base::FilePath fname(FILE_PATH_LITERAL(filename.c_str()));
  // base::FilePath fpath(FILE_PATH_LITERAL(""));
   web_contents->SavePage(fname, fpath, content::SavePageType::SAVE_PAGE_TYPE_AS_ONLY_HTML);


 // GenerateMHTML(path, embedded_test_server()->GetURL("/simple_page.html"));
base::FilePath fullpath = fpath.Append(fname);

std::string contents;
base::ReadFileToString(fullpath, &contents);

//int64_t size(0);
//auto res = base::GetFileSize(fullpath,&size);
LOG(INFO) << "!!!fullpath:" << fullpath << " size: " << file_size() << " Content:" << contents; 

}
*/
}
