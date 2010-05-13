#include "Precompile.h"
#include "BrowserFrame.h"

#include "Browser.h"
#include "BrowserEvents.h"
#include "BrowserPreferencesDialog.h"
#include "BrowserSearch.h"
#include "CollectionManager.h"
#include "CollectionsPanel.h"
#include "FoldersPanel.h"
#include "NavigationPanel.h"
#include "PreviewPanel.h"
#include "ResultsPanel.h"
#include "SearchQuery.h"

#include "Asset/ArtFileAttribute.h"
#include "Asset/AssetFile.h"
#include "Asset/AssetFolder.h"
#include "Asset/AssetInit.h"
#include "Asset/Manager.h"
#include "Asset/ShaderAsset.h"
#include "Asset/Tracker.h"
#include "AssetManager/CreateAssetWizard.h"
#include "Attribute/AttributeHandle.h"
#include "Common/CommandLine.h"
#include "Editor/DocumentManager.h"
#include "Editor/SessionManager.h"
#include "File/Manager.h"
#include "FileSystem/FileSystem.h"
#include "Finder/ContentSpecs.h"
#include "Finder/Finder.h"
#include "RCS/RCS.h"
#include "Scene/SceneManager.h"
#include "UIToolKit/AutoCompleteComboBox.h"
#include "UIToolKit/Button.h"
#include "UIToolKit/ImageManager.h"
#include "UIToolKit/MenuButton.h"
#include "Windows/Error.h"
#include "Windows/Process.h"
#include "Windows/Thread.h"


#include <wx/clipbrd.h>

using namespace Luna;

static const u32 s_ToggleButtonStyle = 
( UIToolKit::ButtonStyles::BU_BITMAP
 | UIToolKit::ButtonStyles::BU_TOGGLE
 | UIToolKit::ButtonStyles::BU_CENTER );

static const char* s_BrowserHelpText = 
"Search for (\"*\" is wildcard): \n" \
"- literal-words: rock\n" \
"- quoted-words: \"ground rock\"\n" \
"- key-words: user: rachel \n" \
"- file paths\n" \
"\n" \
"Key-words:\n" \
"- id: file id (TUID).\n" \
"- name: file name\n" \
"- path: file path \n" \
"- type: file type\n" \
"- user: created-by user.\n" \
"- engine: asset engine type\n" \
"- level: entities that are in level\n" \
"- shader: entities that use shader\n" \
"\n" \
"Examples:\n" \
"  rock*entity.irb\n" \
"  rock level: gimlick\n" \
"  shader:default_pink\n" \
"  id:7117542371602659975\n";


///////////////////////////////////////////////////////////////////////////////
/// Class HelpPanel
///////////////////////////////////////////////////////////////////////////////
class Luna::HelpPanel : public HelpPanelGenerated 
{	
public:
  HelpPanel( BrowserFrame* browserFrame )
    : HelpPanelGenerated( browserFrame )
    , m_BrowserFrame( browserFrame )
  {
  }
  virtual ~HelpPanel()
  {
  }

private:
  BrowserFrame* m_BrowserFrame;
};


///////////////////////////////////////////////////////////////////////////////
/// Class Browser
///////////////////////////////////////////////////////////////////////////////
BEGIN_EVENT_TABLE( BrowserFrame, BrowserFrameGenerated )
EVT_MENU( BrowserMenu::AdvancedSearch, BrowserFrame::OnAdvancedSearch )
EVT_BUTTON( BrowserMenu::AdvancedSearchGo, BrowserFrame::OnAdvancedSearchGoButton )
EVT_BUTTON( BrowserMenu::AdvancedSearchCancel, BrowserFrame::OnAdvancedSearchCancelButton )
EVT_MENU( BrowserMenu::Cut, BrowserFrame::OnCut )
EVT_MENU( BrowserMenu::Copy, BrowserFrame::OnCopy )
EVT_MENU( BrowserMenu::Paste, BrowserFrame::OnPaste )
//EVT_MENU( BrowserMenu::Rename, BrowserFrame::OnRename )
EVT_MENU( BrowserMenu::Delete, BrowserFrame::OnDelete )
EVT_MENU( BrowserMenu::Open, BrowserFrame::OnOpen )
EVT_MENU( BrowserMenu::Preview, BrowserFrame::OnPreview )
EVT_MENU( BrowserMenu::CheckOut, BrowserFrame::OnCheckOut )
EVT_MENU( BrowserMenu::History, BrowserFrame::OnRevisionHistory )
EVT_MENU( BrowserMenu::CopyPathClean, BrowserFrame::OnCopyPath )
EVT_MENU( BrowserMenu::CopyPathWindows, BrowserFrame::OnCopyPath )
EVT_MENU( BrowserMenu::CopyFileIDHex, BrowserFrame::OnCopyFileID )
EVT_MENU( BrowserMenu::CopyFileIDDecimal, BrowserFrame::OnCopyFileID )
EVT_MENU( BrowserMenu::ShowInFolders, BrowserFrame::OnShowInFolders )
EVT_MENU( BrowserMenu::ShowInPerforce, BrowserFrame::OnShowInPerforce )
EVT_MENU( BrowserMenu::ShowInWindowsExplorer, BrowserFrame::OnShowInWindowsExplorer )
EVT_MENU( BrowserMenu::Preferences, BrowserFrame::OnPreferences )
EVT_MENU( BrowserMenu::NewCollectionFromSelection, BrowserFrame::OnNewCollectionFromSelection )
EVT_MENU( BrowserMenu::NewDepedencyCollectionFromSelection, BrowserFrame::OnNewCollectionFromSelection )
EVT_MENU( BrowserMenu::NewUsageCollectionFromSelection, BrowserFrame::OnNewCollectionFromSelection )
igEVT_UPDATE_STATUS( wxID_ANY, BrowserFrame::OnUpdateStatusBar )
END_EVENT_TABLE()


BrowserFrame::BrowserFrame( Browser* browser, BrowserSearch* browserSearch, SearchHistory* searchHistory, wxWindow* parent )
: BrowserFrameGenerated( parent, wxID_ANY, wxT( "Asset Vault" ), wxDefaultPosition, wxSize( 840, 550 ) )//, id, title, pos, size, style )
, m_Browser( browser )
, m_BrowserSearch( browserSearch )
, m_SearchHistory( searchHistory )
, m_PreferencePrefix( "BrowserFrame" )
, m_NavigationPanel( NULL )
, m_ResultsPanel( NULL )
, m_FoldersPanel( NULL )
, m_CollectionsPanel( NULL )
, m_HelpPanel( NULL )
, m_StatusBar( NULL )
, m_CurrentViewOption( ViewOptionIDs::Medium )
, m_OptionsMenu( NULL )
, m_ThumbnailViewMenu( NULL )
, m_PanelsMenu( NULL )
, m_IsSearching( false )
, m_IgnoreFolderSelect( false )
{
#pragma TODO( "Populate the CollectionsPanel from BrowserPreferences" )

  NOC_ASSERT( m_Browser );

  //
  // Set the task bar icon
  //
  wxIconBundle iconBundle;

  wxIcon tempIcon;
  tempIcon.CopyFromBitmap( UIToolKit::GlobalImageManager().GetBitmap( "vault_16.png" ) );
  iconBundle.AddIcon( tempIcon );

  tempIcon.CopyFromBitmap( UIToolKit::GlobalImageManager().GetBitmap( "vault_32.png" ) );
  iconBundle.AddIcon( tempIcon );

  SetIcons( iconBundle );

  //
  // Navigation Tool Bar
  //
  {
    m_NavigationPanel =  new NavigationPanel( this, m_SearchHistory );

    // Add to the AUI Manager
    wxAuiPaneInfo info;
    info.Name( wxT( "NavigationBar" ) );
    info.DestroyOnClose( false );
    info.CaptionVisible( false );
    info.CloseButton( false );
    info.Floatable( false );
    info.Dock();
    info.Top();
    info.Layer( 2 );
    info.PaneBorder( false );
    info.DockFixed( true );

    m_FrameManager.AddPane( m_NavigationPanel, info );
  }

  //
  // Center Results List Ctrl
  //
  {
    m_ResultsPanel = new ResultsPanel( this );

    wxAuiPaneInfo info;
    info.Name( "CenterPanel" );
    info.CenterPane();
    info.Layer( 0 );
    info.PaneBorder( false );    

    m_FrameManager.AddPane( m_ResultsPanel, info );
  }

  //
  // Preview Panel
  //
  {
    m_PreviewPanel = new PreviewPanel( this );

    wxAuiPaneInfo info;
    info.Name( "PreviewPanel" );
    info.DestroyOnClose( false );
    info.Caption( "Preview" );
    info.Dock();
    info.Right();
    info.Layer( 1 );
    info.Floatable( false );
    info.PaneBorder( false );
    info.MinSize( 200, 250 );
    info.BestSize( 250, 300 );

    m_FrameManager.AddPane( m_PreviewPanel, info );
  }

  //
  // Folder Panel
  //
  {
    m_FoldersPanel = new FoldersPanel( this );

    wxAuiPaneInfo info;
    info.Name( wxT( BrowserMenu::Label( BrowserMenu::FoldersPanel ).c_str() ) );
    info.DestroyOnClose( false );
    info.Caption( wxT( BrowserMenu::Label( BrowserMenu::FoldersPanel ).c_str() ) );
    info.Dock();
    info.Left();
    info.Layer( 2 );
    info.Floatable( false );
    info.PaneBorder( false );
    info.MinSize( 200, 250 );
    info.BestSize( 230, 300 );

    m_FrameManager.AddPane( m_FoldersPanel, info );

    std::string defaultPath = m_Browser->GetBrowserPreferences()->GetDefaultFolderPath();
    if ( !defaultPath.empty() )
    {
      m_FoldersPanel->SetPath( defaultPath );
    }

    // Status bar
    m_StatusBar = new BrowserStatusBar( this );
    SetStatusBar( m_StatusBar );

    // Connect Folders
    wxTreeCtrl* tree = m_FoldersPanel->GetTreeCtrl();
    tree->Connect( tree->GetId(), wxEVT_COMMAND_TREE_SEL_CHANGED, wxTreeEventHandler( BrowserFrame::OnFolderSelected ), NULL, this );
    tree->Connect( tree->GetId(), wxEVT_COMMAND_TREE_ITEM_ACTIVATED, wxTreeEventHandler( BrowserFrame::OnFolderSelected ), NULL, this );
  }

  //
  // Collections Panel
  //
  {
    m_CollectionsPanel = new CollectionsPanel( this );

    wxAuiPaneInfo info;
    info.Name( wxT( BrowserMenu::Label( BrowserMenu::CollectionsPanel ).c_str() ) );
    info.DestroyOnClose( false );
    info.Caption( wxT( BrowserMenu::Label( BrowserMenu::CollectionsPanel ).c_str() ) );
    info.Dock();
    info.Right();
    info.Position( 1 );
    info.Layer( 2 );
    info.Floatable( false );
    info.PaneBorder( false );
    info.MinSize( 200, 250 );
    info.BestSize( 250, 300 );

    m_FrameManager.AddPane( m_CollectionsPanel, info );
  }

  //
  // Help Panel
  //
  {
    m_HelpPanel = new HelpPanel( this );

    const wxFont& defaultFont = m_HelpPanel->GetFont();
    m_DefaultTextAttr.SetFont( defaultFont );

    wxFont titleFont( defaultFont );
    titleFont.SetWeight( wxFONTWEIGHT_BOLD );
    m_TitleTextAttr.SetFlags( wxTEXT_ATTR_FONT_FACE );
    m_TitleTextAttr.SetFont( titleFont );

    wxFont msgFont( defaultFont );
    msgFont.SetStyle( wxFONTSTYLE_ITALIC );
    m_ItalicTextAttr.SetFlags( wxTEXT_ATTR_FONT_FACE );
    m_ItalicTextAttr.SetFont( msgFont );

    m_HelpPanel->m_HelpTextCtrl->SetDefaultStyle( m_DefaultTextAttr );
    ( *m_HelpPanel->m_HelpTextCtrl ) << s_BrowserHelpText;

    wxAuiPaneInfo info;
    info.Name( wxT( BrowserMenu::Label( BrowserMenu::HelpPanel ).c_str() ) );
    info.DestroyOnClose( false );
    info.Caption( wxT( BrowserMenu::Label( BrowserMenu::HelpPanel ).c_str() ) );
    info.Dock();
    info.Right();
    info.Position( 3 );
    info.Layer( 2 );
    info.Floatable( false );
    info.PaneBorder( false );
    info.MinSize( 200, 250 );
    info.BestSize( 250, 300 );

    m_FrameManager.AddPane( m_HelpPanel, info );
  }

  m_FrameManager.Update();

  //
  // View Menu Button
  //
  {
    m_OptionsMenu = new wxMenu();

    m_ThumbnailViewMenu = new wxMenu();
    {
      wxMenuItem* smallMenuItem = new wxMenuItem(
        m_ThumbnailViewMenu,
        BrowserMenu::ViewSmall,
        wxT( BrowserMenu::Label( BrowserMenu::ViewSmall ) + std::string( " " ) + ThumbnailSizes::Label( ThumbnailSizes::Small ) ),
        wxT( BrowserMenu::Label( BrowserMenu::ViewSmall ).c_str() ),
        wxITEM_CHECK );
      m_ThumbnailViewMenu->Append( smallMenuItem );
      Connect( BrowserMenu::ViewSmall, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnOptionsMenuSelect ), NULL, this );

      wxMenuItem* mediumMenuItem = new wxMenuItem( 
        m_ThumbnailViewMenu, 
        BrowserMenu::ViewMedium, 
        wxT( BrowserMenu::Label( BrowserMenu::ViewMedium ) + std::string( " " ) + ThumbnailSizes::Label( ThumbnailSizes::Medium ) ),
        wxT( BrowserMenu::Label( BrowserMenu::ViewMedium ) ),
        wxITEM_CHECK );
      m_ThumbnailViewMenu->Append( mediumMenuItem );
      Connect( BrowserMenu::ViewMedium, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnOptionsMenuSelect ), NULL, this );

      wxMenuItem* largeMenuItem = new wxMenuItem(
        m_ThumbnailViewMenu,
        BrowserMenu::ViewLarge,
        wxT( BrowserMenu::Label( BrowserMenu::ViewLarge ) + std::string( " " ) + ThumbnailSizes::Label( ThumbnailSizes::Large ) ),
        wxT( BrowserMenu::Label( BrowserMenu::ViewLarge ) ),
        wxITEM_CHECK );
      m_ThumbnailViewMenu->Append( largeMenuItem );
      Connect( BrowserMenu::ViewLarge, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnOptionsMenuSelect ), NULL, this );
    }

#pragma TODO( "Toggle the Folders and Collections, etc... panels' states in panelsMenu based on loaded preferences." )
    m_PanelsMenu = new wxMenu();
    CreatePanelsMenu( m_PanelsMenu );
    UpdatePanelsMenu( m_PanelsMenu );

    m_OptionsMenu->Append( BrowserMenu::AdvancedSearch, wxT( BrowserMenu::Label( BrowserMenu::AdvancedSearch ) ) );
    m_OptionsMenu->AppendSeparator();
    m_OptionsMenu->Append( wxID_ANY, wxT( "Thumbnail Size" ), m_ThumbnailViewMenu );
    m_OptionsMenu->AppendSeparator();
    m_OptionsMenu->Append( wxID_ANY, wxT( "Show Panels" ), m_PanelsMenu );
    m_OptionsMenu->AppendSeparator();
    m_OptionsMenu->Append( BrowserMenu::Preferences, BrowserMenu::Label( BrowserMenu::Preferences ) );

    m_NavigationPanel->m_OptionsButton->SetContextMenu( m_OptionsMenu );
    m_NavigationPanel->m_OptionsButton->SetHoldDelay( 0.0f );

    m_NavigationPanel->m_OptionsButton->Connect( wxEVT_MENU_OPEN, wxMenuEventHandler( BrowserFrame::OnOptionsMenuOpen ), NULL, this );
    m_NavigationPanel->m_OptionsButton->Connect( wxEVT_MENU_CLOSE, wxMenuEventHandler( BrowserFrame::OnOptionsMenuClose ), NULL, this );
  }

  //
  // Load Preferences
  //
  m_Browser->GetBrowserPreferences()->GetWindowSettings( this, &m_FrameManager );
  m_Browser->GetBrowserPreferences()->AddChangedListener( Reflect::ElementChangeSignature::Delegate( this, &BrowserFrame::OnPreferencesChanged ) );
  m_CurrentViewOption = m_Browser->GetBrowserPreferences()->GetThumbnailMode();
  UpdateResultsView( m_Browser->GetBrowserPreferences()->GetThumbnailSize() );

  //
  // Connect Events
  //
  //Connect( GetId(), wxEVT_CLOSE_WINDOW, wxCloseEventHandler( BrowserFrame::OnClose ), NULL, this );

  // Connect Listeners
  m_BrowserSearch->AddRequestSearchListener( Luna::RequestSearchSignature::Delegate( this, &BrowserFrame::OnRequestSearch ) );
  m_BrowserSearch->AddBeginSearchingListener( Luna::BeginSearchingSignature::Delegate( this, &BrowserFrame::OnBeginSearching ) );
  m_BrowserSearch->AddResultsAvailableListener( Luna::ResultsAvailableSignature::Delegate( this, &BrowserFrame::OnResultsAvailable ) );
  m_BrowserSearch->AddSearchCompleteListener( Luna::SearchCompleteSignature::Delegate( this, &BrowserFrame::OnSearchComplete ) );
  
  m_ResultsPanel->AddResultsChangedListener( ResultSignature::Delegate( this, &BrowserFrame::OnResultsPanelUpdated ) );

  m_StatusBar->UpdateTrackerStatus( Asset::GlobalTracker()->IsTracking() );
}

BrowserFrame::~BrowserFrame()
{
  // Disconnect Events
  //Disconnect( GetId(), wxEVT_CLOSE_WINDOW, wxCloseEventHandler( BrowserFrame::OnClose ), NULL, this );

  // Disconnect ViewMenu
  Disconnect( BrowserMenu::ViewSmall, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnOptionsMenuSelect ), NULL, this );
  Disconnect( BrowserMenu::ViewMedium, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnOptionsMenuSelect ), NULL, this );
  Disconnect( BrowserMenu::ViewLarge, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnOptionsMenuSelect ), NULL, this );

  // Disconnect Folders
  wxTreeCtrl* tree = m_FoldersPanel->GetTreeCtrl();
  tree->Disconnect( tree->GetId(), wxEVT_COMMAND_TREE_SEL_CHANGED, wxTreeEventHandler( BrowserFrame::OnFolderSelected ), NULL, this );
  tree->Disconnect( tree->GetId(), wxEVT_COMMAND_TREE_ITEM_ACTIVATED, wxTreeEventHandler( BrowserFrame::OnFolderSelected ), NULL, this );

  // Disconnect Listeners
  m_BrowserSearch->RemoveRequestSearchListener( Luna::RequestSearchSignature::Delegate( this, &BrowserFrame::OnRequestSearch ) );
  m_BrowserSearch->RemoveBeginSearchingListener( Luna::BeginSearchingSignature::Delegate( this, &BrowserFrame::OnBeginSearching ) );
  m_BrowserSearch->RemoveResultsAvailableListener( Luna::ResultsAvailableSignature::Delegate( this, &BrowserFrame::OnResultsAvailable ) );
  m_BrowserSearch->RemoveSearchCompleteListener( Luna::SearchCompleteSignature::Delegate( this, &BrowserFrame::OnSearchComplete ) );

  m_ResultsPanel->RemoveResultsChangedListener( ResultSignature::Delegate( this, &BrowserFrame::OnResultsPanelUpdated ) );
}

/////////////////////////////////////////////////////////////////////////////
void BrowserFrame::SaveWindowState()
{
}

/////////////////////////////////////////////////////////////////////////////
const std::string& BrowserFrame::GetPreferencePrefix() const
{
  return m_PreferencePrefix;
}

///////////////////////////////////////////////////////////////////////////////
// Sets the string in the NavBar and starts the search query.
void BrowserFrame::Search( const std::string& queryString, const AssetCollection* collection, const std::string& selectPath )
{
  wxBusyCursor bc;
  if ( queryString.empty() && !collection )
    return;

  std::string errors;
  if ( !SearchQuery::ParseQueryString( queryString, errors ) )
  {
    wxMessageBox( errors.c_str(), "Search Errors", wxCENTER | wxICON_WARNING | wxOK, this );
    return;
  }

  m_SearchHistory->RunNewQuery( queryString, collection, selectPath );
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::GetSelectedFilesAndFolders( Asset::V_AssetFiles& files, Asset::V_AssetFolders& folders )
{
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );
}

///////////////////////////////////////////////////////////////////////////////
bool BrowserFrame::IsPreviewable( const Asset::AssetFile* file )
{
  if ( file )
  {
    Asset::AssetClassPtr asset = Asset::AssetFile::GetAssetClass( file );
    if ( asset.ReferencesObject() )
    {
      Attribute::AttributeViewer< Asset::ArtFileAttribute > artFile( asset );
      if ( artFile.Valid() && artFile->m_FileID != TUID::Null )
      {
        std::string path = File::GlobalManager().GetPath( artFile->m_FileID );
        if ( !path.empty() )
        {
          std::string exportFile = FinderSpecs::Content::STATIC_DECORATION.GetExportFile( path, artFile->m_FragmentNode );
          return FileSystem::Exists( exportFile );
        }
      }
    }
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
wxMenu* BrowserFrame::GetNewAssetMenu( bool forceEnableAll )
{
  bool enableItems = forceEnableAll || InFolder();

  if ( !m_MenuItemToAssetType.empty() )
  {
    M_i32::const_iterator itr = m_MenuItemToAssetType.begin();
    M_i32::const_iterator end = m_MenuItemToAssetType.end();
    for ( ; itr != end; ++itr )
    {
      i32 menuItemID = itr->first;
      Disconnect( menuItemID, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnNew ), NULL, this );
    }
  }

  m_MenuItemToAssetType.clear();

  wxMenu* newMenu = new wxMenu();

  // Menu for creating new assets
  typedef std::map< const Reflect::Class*, wxMenu* > M_SubMenus;
  M_SubMenus subMenus;

  // Add a submenu for shaders since there's so many different kinds.
  // Additional submenus could be added here as well.
  const Reflect::Class* shaderBase = Reflect::GetClass< Asset::ShaderAsset >();
  wxMenu* shaderSubMenu = subMenus.insert( M_SubMenus::value_type( shaderBase, new wxMenu() ) ).first->second;

  wxMenuItem* shaderSubMenuItem = new wxMenuItem( newMenu, wxID_ANY, shaderBase->m_UIName.c_str(), shaderBase->m_UIName.c_str(), wxITEM_NORMAL, shaderSubMenu );
  newMenu->Append( shaderSubMenuItem );
  shaderSubMenuItem->Enable( enableItems );

  // Populate the New asset menu
  V_i32::const_iterator assetItr = Asset::g_AssetClassTypes.begin();
  V_i32::const_iterator assetEnd = Asset::g_AssetClassTypes.end();
  for ( ; assetItr != assetEnd; ++assetItr )
  {
    const i32 typeID = (*assetItr);
    const Reflect::Class* typeInfo = Reflect::Registry::GetInstance()->GetClass( typeID );

    wxMenuItem* menuItem = NULL;

    M_SubMenus::const_iterator foundSubMenu = subMenus.find( Reflect::Registry::GetInstance()->GetClass( typeInfo->m_Base ) );
    if ( foundSubMenu != subMenus.end() )
    {
      menuItem = foundSubMenu->second->Append( wxID_ANY, typeInfo->m_UIName.c_str() );
    }
    else
    {
      menuItem = newMenu->Append( wxID_ANY, typeInfo->m_UIName.c_str() );
    }

    menuItem->Enable( enableItems );

    // Map the menu item ID to the asset class ID so that when we get a menu item
    // callback, we know which type of asset to create.
    m_MenuItemToAssetType.insert( M_i32::value_type( menuItem->GetId(), typeID ) );

    // Connect a callback for when the menu item is selected.  No need to disconnect
    // this handler since the lifetime of this class is tied to the menu.
    Connect( menuItem->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( BrowserFrame::OnNew ), NULL, this );
  }

  return m_MenuItemToAssetType.empty() ? NULL : newMenu;
}

///////////////////////////////////////////////////////////////////////////////
bool BrowserFrame::InFolder()
{
  const SearchQuery* curQuery = m_SearchHistory->GetCurrentQuery();
  if ( curQuery )
  {
    return ( curQuery->GetSearchType() == SearchTypes::Folder );
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Called when a menu is about to be opened.  Enables and disables items in the
// menu as appropriate.
// 
void BrowserFrame::OnOptionsMenuOpen( wxMenuEvent& event )
{
  event.Skip();
  if ( event.GetMenu() == m_OptionsMenu )
  {
    UpdatePanelsMenu( m_PanelsMenu );
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnOptionsMenuClose( wxMenuEvent& event )
{
  m_NavigationPanel->m_NavBarComboBox->SetFocus();
  event.Skip();
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnOptionsMenuSelect( wxCommandEvent& event )
{
  event.Skip();

  ViewOptionID id = (ViewOptionID) event.GetId();
  m_CurrentViewOption = id;
  UpdateResultsView();
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnFolderSelected( wxTreeEvent& event )
{
  if ( m_IgnoreFolderSelect )
    return;

  std::string queryString;
  m_FoldersPanel->GetPath( queryString );
  if ( !queryString.empty() )
  {
    FileSystem::CleanName( queryString );
    FileSystem::GuaranteeSlash( queryString );
    NOC_ASSERT( FileSystem::HasPrefix( File::GlobalManager().GetManagedAssetsRoot(), queryString ) );
    Search( queryString );
    event.Skip();
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnAdvancedSearch( wxCommandEvent& event )
{
  m_ResultsPanel->SetViewMode( ViewModes::AdvancedSearch );
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnAdvancedSearchGoButton( wxCommandEvent& event )
{
  // TODO: submit the form
  m_ResultsPanel->SetViewMode( ViewModes::Thumbnail );
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnAdvancedSearchCancelButton( wxCommandEvent& event )
{
  m_ResultsPanel->SetViewMode( ViewModes::Thumbnail );
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnOpen( wxCommandEvent& event )
{
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders; // ignored
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );
  for ( Asset::V_AssetFiles::const_iterator fileItr = files.begin(), fileEnd = files.end();
    fileItr != fileEnd; ++fileItr )
  {
    std::string path = ( *fileItr )->GetFilePath();
    if ( !path.empty() && FileSystem::Exists( path ) )
    {
      SessionManager::GetInstance()->Edit( path );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnPreview( wxCommandEvent& event )
{
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders; // ignored
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );
  if ( !files.empty() )
  {
    Asset::AssetFile* file = *files.begin();
    Asset::AssetClassPtr asset = Asset::AssetFile::GetAssetClass( file );

    if ( !asset.ReferencesObject() )
    {
      std::ostringstream msg;
      msg << "Failed to load asset '" << file->GetFilePath() << "'. Unable to show preview.";
      wxMessageBox( msg.str(), "Error", wxOK | wxCENTER | wxICON_ERROR, this );
      return;
    }

    // Load the preview
    m_PreviewPanel->Preview( asset );

    // Show preview window if it's not already showing
    wxAuiPaneInfo& settings = m_FrameManager.GetPane( m_PreviewPanel );
    if ( !settings.IsShown() )
    {
      settings.Show();
      m_FrameManager.Update();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnSync( wxCommandEvent& event )
{
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders;
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );

  // Sync the files
  for ( Asset::V_AssetFiles::const_iterator fileItr = files.begin(), fileEnd = files.end();
    fileItr != fileEnd; ++fileItr )
  {
    try
    {
      RCS::File rcsFile( ( *fileItr )->GetFilePath() );
      rcsFile.Sync();
    }
    catch ( const Nocturnal::Exception& e )
    {
      wxMessageBox( e.what(), "Sync Failed!", wxCENTER | wxICON_ERROR | wxOK, this );
    }
  }

  // Sync the folders
  for ( Asset::V_AssetFolders::const_iterator folderItr = folders.begin(), folderEnd = folders.end();
    folderItr != folderEnd; ++folderItr )
  {
    std::string path = ( *folderItr )->GetFullPath();
    FileSystem::GuaranteeSlash( path );
    FileSystem::AppendPath( path, "..." );

    try
    {
      RCS::File rcsFile( path );
      rcsFile.Sync();
    }
    catch ( const Nocturnal::Exception& e )
    {
      wxMessageBox( e.what(), "Sync Failed!", wxCENTER | wxICON_ERROR | wxOK, this );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnCheckOut( wxCommandEvent& event )
{
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders;
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );
  if ( !folders.empty() )
  {
    if ( wxYES != 
      wxMessageBox( "Your selection includes folders.  Are you sure that you want to check out all of the contents of the selected folders?  This could result in checking out a lot of files.", 
      "Check Out Folders?", 
      wxCENTER | wxYES_NO | wxICON_WARNING,
      this ) )
    {
      // Operation cancelled
      return;
    }
  }

  // Check out the files
  for ( Asset::V_AssetFiles::const_iterator fileItr = files.begin(), fileEnd = files.end();
    fileItr != fileEnd; ++fileItr )
  {
    try
    {
      RCS::File rcsFile( ( *fileItr )->GetFilePath() );
      rcsFile.Edit();
    }
    catch ( const Nocturnal::Exception& e )
    {
      wxMessageBox( e.what(), "Check Out Failed!", wxCENTER | wxICON_ERROR | wxOK, this );
    }
  }

  // Check out the folders
  for ( Asset::V_AssetFolders::const_iterator folderItr = folders.begin(), folderEnd = folders.end();
    folderItr != folderEnd; ++folderItr )
  {
    std::string path = ( *folderItr )->GetFullPath();
    FileSystem::GuaranteeSlash( path );
    FileSystem::AppendPath( path, "..." );

    try
    {
      RCS::File rcsFile( path );
      rcsFile.Edit();
    }
    catch ( const Nocturnal::Exception& e )
    {
      wxMessageBox( e.what(), "Check Out Failed!", wxCENTER | wxICON_ERROR | wxOK, this );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnRevisionHistory( wxCommandEvent& event )
{
  V_string paths;
  if ( m_ResultsPanel->GetSelectedPaths( paths ) == 1 )
  {
    std::string path = paths.front();
    std::string command = std::string( "p4win.exe -H \"" ) + path + std::string( "\"" );

    try 
    {
      Windows::Execute( command );
    }
    catch ( const Windows::Exception& e )
    {
      std::string error = e.Get();
      error += "\nMake sure that you have p4win properly installed.";
      wxMessageBox( error.c_str(), "Error", wxCENTER | wxICON_ERROR | wxOK, this );
      return;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnCopyPath( wxCommandEvent& event )
{
  V_string paths;
  if ( m_ResultsPanel->GetSelectedPaths( paths, event.GetId() == BrowserMenu::CopyPathClean ) )
  {
    wxString text;
    wxTextDataObject* dataObject = new wxTextDataObject();
    for ( V_string::const_iterator pathItr = paths.begin(),
      pathEnd = paths.end(); pathItr != pathEnd; ++pathItr )
    {
      if ( !text.empty() )
      {
        text += "\n";
      }
      text += *pathItr;
    }

    if ( wxTheClipboard->Open() )
    {
      wxTheClipboard->SetData( new wxTextDataObject( text ) );
      wxTheClipboard->Close();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnCopyFileID( wxCommandEvent& event )
{
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders; // Not used, only files have tuids
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );

  const u32 maxChars = 32;
  char buffer[maxChars];
  if ( !files.empty() )
  {
    wxString text;
    std::string format;
    if ( event.GetId() == BrowserMenu::CopyFileIDHex )
    {
      format = TUID_HEX_FORMAT;
    }
    else
    {
      format = TUID_INT_FORMAT;
    }

    for ( Asset::V_AssetFiles::const_iterator fileItr = files.begin(), fileEnd = files.end();
      fileItr != fileEnd; ++fileItr )
    {
      Asset::AssetFile* file = *fileItr;
      sprintf_s( buffer, maxChars, format.c_str(), file->GetFileID() );

      if ( !text.empty() )
      {
        text += "\n";
      }
      text += buffer;
    }

    if ( !text.empty() )
    {
      wxTextDataObject* dataObject = new wxTextDataObject();
      if ( wxTheClipboard->Open() )
      {
        wxTheClipboard->SetData( new wxTextDataObject( text ) );
        wxTheClipboard->Close();
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnShowInFolders( wxCommandEvent& event )
{
  V_string paths;
  if ( m_ResultsPanel->GetSelectedPaths( paths )  == 1 )
  {
    wxBusyCursor bc;

    std::string path = paths.front();
    if ( FileSystem::Exists( path ) )
    {
      Search( path );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnShowInPerforce( wxCommandEvent& event )
{
  V_string paths;
  if ( m_ResultsPanel->GetSelectedPaths( paths )  == 1 )
  {
    std::string path = paths.front();
    std::string command = std::string( "p4win.exe -s \"" ) + path + std::string( "\"" );

    try 
    {
      Windows::Execute( command );
    }
    catch ( const Windows::Exception& e )
    {
      std::string error = e.Get();
      error += "\nMake sure that you have p4win properly installed.";
      wxMessageBox( error.c_str(), "Error", wxCENTER | wxICON_ERROR | wxOK, this );
      return;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnShowInWindowsExplorer( wxCommandEvent& event )
{
  V_string paths;
  if ( m_ResultsPanel->GetSelectedPaths( paths ) == 1 )
  {
    std::string command = "explorer.exe ";
    std::string path = paths.front();
    if ( FileSystem::IsFile( path ) )
    {
      command += "/select,";
    }
    FileSystem::Win32Name( path );
    command += "\"" + path + "\"";

    try 
    {
      Windows::Execute( command );
    }
    catch ( const Windows::Exception& )
    {
      // Do nothing
      return;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnNewCollectionFromSelection( wxCommandEvent& event )
{
  wxBusyCursor busyCursor;
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders;
  GetSelectedFilesAndFolders( files, folders );
  AssetCollectionPtr collection;

  if ( files.size() )
  {
    if ( event.GetId() == BrowserMenu::NewCollectionFromSelection )
    {
      collection = new AssetCollection( "New Collection", AssetCollectionFlags::CanRename | AssetCollectionFlags::CanHandleDragAndDrop );
      S_tuid fileIDs;
      for ( Asset::V_AssetFiles::const_iterator fileItr = files.begin(), fileEnd = files.end();
        fileItr != fileEnd; ++fileItr )
      {
        const tuid& id = ( *fileItr )->GetFileID();
        if ( id != TUID::Null )
        {
          fileIDs.insert( id );
        }
      }

      if ( !fileIDs.empty() )
      {
        collection->SetAssetIDs( fileIDs );
      }
    }
    else
    {
      const bool reverse = event.GetId() == BrowserMenu::NewUsageCollectionFromSelection;
      Asset::AssetFile* file = *files.begin();
      DependencyCollectionPtr dependencyCollection = new DependencyCollection( file->GetShortName(), AssetCollectionFlags::Dynamic, reverse );
      dependencyCollection->SetRootID( file->GetFileID() );
      dependencyCollection->LoadDependencies();
      collection = dependencyCollection;
    }
  }

  if ( collection )
  {
    CollectionManager* collectionManager = m_Browser->GetBrowserPreferences()->GetCollectionManager();
    std::string name;
    collectionManager->GetUniqueName( name, collection->GetName().c_str() );
    collection->SetName( name );
    collectionManager->AddCollection( collection );
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnRefresh( wxCommandEvent& args )
{
  m_SearchHistory->RunCurrentQuery();
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnNew( wxCommandEvent& args )
{
  M_i32::const_iterator found = m_MenuItemToAssetType.find( args.GetId() );
  if ( found != m_MenuItemToAssetType.end() )
  {
    const i32 assetTypeID = found->second;

    ::AssetManager::CreateAssetWizard wizard( this, assetTypeID );

    std::string defaultDir;
    const SearchQuery* curQuery = m_SearchHistory->GetCurrentQuery();
    if ( curQuery 
      && ( curQuery->GetSearchType() == SearchTypes::Folder ) )
    {
      wizard.SetDefaultDirectory( curQuery->GetQueryString() );

      if ( wizard.Run() )
      {
        wxBusyCursor bc;

        Asset::AssetClassPtr assetClass = wizard.GetAssetClass();
        if ( !assetClass.ReferencesObject() )
        {
          Console::Error( "CreateAssetWizard returned a NULL asset when attempting to create new asset at location %s.", wizard.GetNewFileLocation().c_str() );
        }
        else
        {
          // re-run the search query to show the new asset
          m_SearchHistory->RunNewQuery( wizard.GetNewFileLocation(), NULL );
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnNewFolder( wxCommandEvent& args )
{

}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnCut( wxCommandEvent& args )
{

}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnCopy( wxCommandEvent& args )
{

}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnPaste( wxCommandEvent& args )
{

}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnDelete( wxCommandEvent& args )
{
  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders;
  GetSelectedFilesAndFolders( files, folders );
  if ( files.size() == 1 && folders.empty() )
  {
    Asset::AssetFile* assetFile = files.front();

    std::string errors;
    if ( File::GlobalManager().ValidateCanDeleteFile( assetFile->m_Path, File::ManagerConfigs::Default, File::GlobalManager().GetManagedAssetsRoot(), errors ) )
    {
      wxString msg( "Are you sure you would like to permanently delete the asset file:\n" );
      msg += assetFile->m_Path;
      if ( wxMessageBox( msg, "Delete Asset?", wxYES_NO | wxCENTER | wxICON_QUESTION, this ) == wxYES )
      {
        RCS::Changeset changeset;
        changeset.m_Description = "Deleting Asset: ";
        changeset.m_Description += assetFile->m_Path;
        changeset.Create();

        File::GlobalManager().SetWorkingChangeset( changeset );
        File::GlobalManager().BeginTrans( File::ManagerConfigs::Default, false );
        try
        {
          S_string relatedFiles;
          Asset::Manager::GetRelatedFiles( assetFile, relatedFiles );

          File::ManagerConfig managerConfig = File::ManagerConfigs::Default;
          File::GlobalManager().Delete( assetFile->m_Path, managerConfig );

          File::ManagerConfig relatedManagerConfig = ( managerConfig & ~File::ManagerConfigs::Resolver );
          File::GlobalManager().Delete( relatedFiles, relatedManagerConfig );

          File::GlobalManager().CommitTrans();

          if ( !Nocturnal::GetCmdLineFlag( File::Args::NoAutoSubmit ) )
          {
            changeset.Commit();
          }
          
          changeset.Clear();
          File::GlobalManager().SetWorkingChangeset( changeset );
        }
        catch ( const Nocturnal::Exception& e )
        {
          std::stringstream str;
          str << "Failed to delete asset: " << e.what();
          wxMessageBox( str.str().c_str(), "Error", wxCENTER | wxICON_ERROR | wxOK, this );

          File::GlobalManager().RollbackTrans();
          changeset.Revert();
          changeset.Clear();
          File::GlobalManager().SetWorkingChangeset( changeset );
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnPreferences( wxCommandEvent& event )
{
  BrowserPreferencesDialog dlg( this );
  dlg.ShowModal();
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnUpdateStatusBar( Luna::UpdateStatusEvent& event )
{
  m_StatusBar->SetStatusText( event.GetText() );
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::OnClose( wxCloseEvent& event )
{
  event.Skip();

  m_Browser->GetBrowserPreferences()->RemoveChangedListener( Reflect::ElementChangeSignature::Delegate( this, &BrowserFrame::OnPreferencesChanged ) );

  m_Browser->GetBrowserPreferences()->SetThumbnailMode( m_CurrentViewOption );
  m_Browser->GetBrowserPreferences()->SetWindowSettings( this, &m_FrameManager );

  std::string path;
  m_FoldersPanel->GetPath( path );
  m_Browser->GetBrowserPreferences()->SetDefaultFolderPath( path );
  m_Browser->OnCloseBrowser();
}

///////////////////////////////////////////////////////////////////////////////
// Callback for when search is requested
// 
void BrowserFrame::OnRequestSearch( const Luna::RequestSearchArgs& args )
{
  UpdateNavBar( args.m_SearchQuery );
  m_ResultsPanel->ClearResults();
}

///////////////////////////////////////////////////////////////////////////////
// Callback for when search is starting.
// 
void BrowserFrame::OnBeginSearching( const Luna::BeginSearchingArgs& args )
{
  m_IsSearching = true;
  m_ResultsPanel->ClearResults();
}

///////////////////////////////////////////////////////////////////////////////
// Callback for when search results are ready to be displayed.
// 
void BrowserFrame::OnResultsAvailable( const Luna::ResultsAvailableArgs& args )
{
  m_ResultsPanel->SetResults( args.m_SearchResults );
}

///////////////////////////////////////////////////////////////////////////////
// Callback for when search is done - update status bar and other UI elements.
// 
void BrowserFrame::OnSearchComplete( const Luna::SearchCompleteArgs& args )
{
  m_IsSearching = false;
#pragma TODO ("Rachel: figure out how to add EndSearch listener and hook up ResultsPanel::SelectPath to select this path.")
  if ( !args.m_SearchQuery->GetSelectPath().empty() )
  {
    m_ResultsPanel->SelectPath( args.m_SearchQuery->GetSelectPath() );
    m_FoldersPanel->SetPath( args.m_SearchQuery->GetSelectPath() );
  }

  Asset::V_AssetFiles files;
  Asset::V_AssetFolders folders;
  m_ResultsPanel->GetSelectedFilesAndFolders( files, folders );
  u32 numFolders = m_ResultsPanel->GetNumFolders();
  u32 numFiles = m_ResultsPanel->GetNumFiles();
  UpdateStatusBar( numFolders, numFiles, files.size() + folders.size(), "" );
}

///////////////////////////////////////////////////////////////////////////////
// Update the UI when the preferences change
// 
void BrowserFrame::OnPreferencesChanged( const Reflect::ElementChangeArgs& args )
{
  m_CurrentViewOption = m_Browser->GetBrowserPreferences()->GetThumbnailMode();
  UpdateResultsView( m_Browser->GetBrowserPreferences()->GetThumbnailSize() );
}

///////////////////////////////////////////////////////////////////////////////
// Callback for changes to the ResultsPanel.  Updates the status bar accordingly.
// 
void BrowserFrame::OnResultsPanelUpdated( const ResultChangeArgs& args )
{
  u32 numFolders = m_ResultsPanel->GetNumFolders();
  u32 numFiles = m_ResultsPanel->GetNumFiles();
  UpdateStatusBar( numFolders, numFiles, args.m_NumSelected, args.m_HighlightPath );
}

///////////////////////////////////////////////////////////////////////////////
// Disconnect folder events and set the path
// 
void BrowserFrame::SetFolderPath( const std::string& folderPath )
{
  m_IgnoreFolderSelect = true;
  m_FoldersPanel->SetPath( folderPath );
  m_IgnoreFolderSelect = false;
}


///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::UpdateNavBar( const SearchQueryPtr& searchQuery )
{
  bool isFolder = false;
  if ( searchQuery->GetSearchType() == SearchTypes::File )
  {
    std::string folder = searchQuery->GetQueryString();
    FileSystem::StripLeaf( folder );
    SetFolderPath( folder );
  }
  else if ( searchQuery->GetSearchType() == SearchTypes::Folder )
  {
    isFolder = true;
    SetFolderPath( searchQuery->GetQueryString() );
  }
  else
  {
    m_FoldersPanel->Unselect();
  }

  const std::string& queryString = searchQuery->GetQueryString();
  m_NavigationPanel->SetNavBarValue( queryString, isFolder );
}

///////////////////////////////////////////////////////////////////////////////
void BrowserFrame::UpdateResultsView( u16 customSize )
{
  m_ThumbnailViewMenu->Check( BrowserMenu::ViewSmall, false );
  m_ThumbnailViewMenu->Check( BrowserMenu::ViewMedium, false );
  m_ThumbnailViewMenu->Check( BrowserMenu::ViewLarge, false );

  switch( m_CurrentViewOption )
  {
  case ViewOptionIDs::Small:
    m_ResultsPanel->SetViewMode( ViewModes::Thumbnail );
    m_ResultsPanel->SetThumbnailSize( ThumbnailSizes::Small );
    m_ThumbnailViewMenu->Check( m_CurrentViewOption, true );
    break;

  case ViewOptionIDs::Medium:
    m_ResultsPanel->SetViewMode( ViewModes::Thumbnail );
    m_ResultsPanel->SetThumbnailSize( ThumbnailSizes::Medium );
    m_ThumbnailViewMenu->Check( m_CurrentViewOption, true );
    break;

  case ViewOptionIDs::Large:
    m_ResultsPanel->SetViewMode( ViewModes::Thumbnail );
    m_ResultsPanel->SetThumbnailSize( ThumbnailSizes::Large );
    m_ThumbnailViewMenu->Check( m_CurrentViewOption, true );
    break;

  default:
    m_ResultsPanel->SetThumbnailSize( customSize );
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Sets the status bar message (main area, left side of status bar) based on
// current search progress.
// 
void BrowserFrame::UpdateStatusBar( size_t numFolders, size_t numFiles, size_t numSelected, const std::string& hover )
{
  std::stringstream status;

  if ( !hover.empty() )
  {
    status << hover;
  }
  else
  {
    if ( m_IsSearching )
    {
      status << "Searching... ";
    }

    if ( !m_IsSearching && !numFolders && !numFiles )
    {
      status << "No results found";
    }
    else
    {
      if ( numFolders )
      {
        status << numFolders << " folder";
        if ( numFolders > 1 )
        {
          status << "s";
        }
      }

      if ( numFiles )
      {
        if ( numFolders )
        {
          status << ", ";
        }
        status << numFiles << " file";
        if ( numFiles > 1 )
        {
          status << "s";
        }
      }

      if ( numSelected )
      {
        if ( numFolders || numFiles )
        {
          status << ", ";
        }
        status << numSelected << " selected";
      }
    }

    if ( Asset::GlobalTracker()->IsTracking() )
    {
      if ( !status.str().empty() )
      {
        status << " ";
      }
      status << "(WARNING: Indexing is in progress, results may be incomplete)";
    }
    else if ( Asset::GlobalTracker()->DidIndexingFail() )
    {
      if ( !status.str().empty() )
      {
        status << " ";
      }
      status << "(WARNING: Indexing FAILED! Results may be incomplete.)";
    }
  }

  Luna::UpdateStatusEvent evt;
  evt.SetEventObject( this );
  evt.SetText( status.str() );
  wxPostEvent( this, evt );
}
