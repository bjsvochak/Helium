#pragma once

#include "API.h"
#include "Core/PropertiesManager.h"
#include "Frame.h"
#include "EditorInfo.h"
#include "EditorState.h"
#include "TUID/TUID.h"
#include "Undo/Command.h"

namespace Luna
{
  class DocumentManager;
  class Editor;

  class PropertiesCreatedCommand : public Undo::Command
  {
  public:
    PropertiesCreatedCommand( PropertiesManager* propertiesManager, u32 selectionId, const Inspect::V_Control& controls ) 
    : m_PropertiesManager( propertiesManager )
    , m_SelectionId( selectionId )
    , m_Controls( controls )
    { 
    }

    virtual void Undo() NOC_OVERRIDE
    {
      // this should never happen
      NOC_BREAK();
    }

    virtual void Redo() NOC_OVERRIDE
    {
      m_PropertiesManager->FinalizeProperties( m_SelectionId, m_Controls );
    }

  private:
     PropertiesManager* m_PropertiesManager;
     u32 m_SelectionId;
     Inspect::V_Control m_Controls;
  };

  /////////////////////////////////////////////////////////////////////////////
  // Base class for different editors in Luna.  Think of each editor as a top
  // level window.
  // 
  class LUNA_EDITOR_API Editor NOC_ABSTRACT : public Frame
  {
  private:
    EditorType  m_EditorType;

    mutable std::string m_PreferencePrefix;

  public:
    Editor( EditorType editorType, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE, const wxString& name = "frame" );
    virtual ~Editor();

    virtual EditorStatePtr GetSessionState();
    void PromptSaveSession( bool forceSaveAs = false );
    void PromptLoadSession();
    virtual bool SaveSession( const EditorStatePtr& state );
    virtual bool LoadSession( const EditorStatePtr& state );

    EditorType GetEditorType() const;

    virtual DocumentManager* GetDocumentManager() = 0;

    virtual const std::string& GetPreferencePrefix() const NOC_OVERRIDE;

    void RevisionHistory( const std::string& path );

  protected:
    void OnPropertiesCreated( const PropertiesCreatedArgs& args );

    DECLARE_EVENT_TABLE();
  };

  typedef std::vector< Editor* > V_EditorDumbPtr;
}
