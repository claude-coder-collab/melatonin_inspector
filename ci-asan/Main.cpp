#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_inspector/melatonin_inspector.h>

#include <cstdio>
#include <functional>
#include <vector>

namespace
{
    class DocumentContent final : public juce::Component
    {
    public:
        explicit DocumentContent (int number)
        {
            setName ("Document " + juce::String (number));
            label.setText (getName(), juce::dontSendNotification);
            addAndMakeVisible (label);
            addAndMakeVisible (button);
            setSize (360, 200);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12);
            label.setBounds (area.removeFromTop (28));
            button.setBounds (area.removeFromTop (28).removeFromLeft (120));
        }

    private:
        juce::Label label;
        juce::TextButton button { "Press me" };
    };

    class Panel final : public juce::MultiDocumentPanel
    {
    public:
        Panel() { setName ("Panel"); }

        void tryToCloseDocumentAsync (juce::Component*, std::function<void (bool)> callback) override
        {
            callback (true);
        }
    };

    template <typename T>
    T* findChild (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* found = dynamic_cast<T*> (child))
                return found;
            if (auto* found = findChild<T> (*child))
                return found;
        }
        return nullptr;
    }

    melatonin::ComponentTreeViewItem* subItem (juce::TreeViewItem& item, int index)
    {
        return dynamic_cast<melatonin::ComponentTreeViewItem*> (item.getSubItem (index));
    }

    melatonin::ComponentTreeViewItem* findItem (melatonin::ComponentTreeViewItem& item, const juce::String& name)
    {
        if (item.getComponentName() == name)
            return &item;
        for (int i = 0; i < item.getNumSubItems(); ++i)
            if (auto* child = subItem (item, i))
                if (auto* found = findItem (*child, name))
                    return found;
        return nullptr;
    }

    class Test final : public juce::Component, private juce::Timer
    {
    public:
        explicit Test (std::function<void (bool)> finished) : onFinished (std::move (finished))
        {
            addAndMakeVisible (panel);
            for (int i = 0; i < 3; ++i)
                addDocument();
            setSize (800, 520);

            steps = {
                [] {},
                [this] { panel.setActiveDocument (panel.getDocument (0)); },
                [this] { addDocument(); },
                [this] { prepareClick(); },
                [this] { clickPreparedItem(); },
                [this] { setLayout (juce::MultiDocumentPanel::FloatingWindows); },
                [this] { addDocument(); },
                [this] { closeActive(); },
                [this] { setLayout (juce::MultiDocumentPanel::MaximisedWindowsWithTabs); },
                [this] { panel.useFullscreenWhenOneDocument (true); },
                [this] { closeAllButOne(); },
                [this] { addDocument(); },
                [this] { addDocument(); },
                [this] { panel.useFullscreenWhenOneDocument (false); },
                [this] { closeAllButOne(); },
                [this] { addDocument(); },
            };
        }

        void start()
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->setVisible (true);
            startTimer (100);
        }

        void resized() override { panel.setBounds (getLocalBounds()); }

    private:
        static constexpr int rounds = 3;

        Panel panel;
        std::unique_ptr<melatonin::Inspector> inspector;
        std::vector<std::function<void()>> steps;
        std::function<void (bool)> onFinished;
        juce::String pendingClick;
        size_t step = 0;
        int nextDocument = 1;
        int clicks = 0;
        int rawChecks = 0;
        int tabChecks = 0;

        void timerCallback() override
        {
            if (step == steps.size() * rounds)
            {
                stopTimer();
                const auto ok = clicks == rounds && rawChecks > 0 && tabChecks > 0;
                if (!ok)
                    report ("expected scenarios didn't all run");
                report (juce::String::formatted ("clicks %d, tabbed checks %d, untabbed checks %d", clicks, tabChecks, rawChecks));
                onFinished (ok);
                return;
            }

            steps[step % steps.size()]();
            ++step;

            if (auto failure = verify(); failure.isNotEmpty())
            {
                stopTimer();
                report ("step " + juce::String (step - 1) + ": " + failure);
                onFinished (false);
            }
        }

        static void report (const juce::String& message)
        {
            std::fprintf (stderr, "%s\n", message.toRawUTF8());
            std::fflush (stderr);
        }

        void addDocument()
        {
            panel.addDocument (new DocumentContent (nextDocument++), juce::Colours::darkslategrey, true);
        }

        void closeActive()
        {
            if (auto* doc = panel.getActiveDocument())
                panel.closeDocumentAsync (doc, false, nullptr);
        }

        void closeAllButOne()
        {
            while (panel.getNumDocuments() > 1)
                panel.closeDocumentAsync (panel.getDocument (0), false, nullptr);
        }

        void setLayout (juce::MultiDocumentPanel::LayoutMode mode)
        {
            panel.setLayoutMode (mode);
        }

        juce::TreeView* tree() const { return inspector != nullptr ? findChild<juce::TreeView> (*inspector) : nullptr; }

        melatonin::ComponentTreeViewItem* rootItem() const
        {
            auto* t = tree();
            return t != nullptr ? dynamic_cast<melatonin::ComponentTreeViewItem*> (t->getRootItem()) : nullptr;
        }

        void prepareClick()
        {
            pendingClick = {};
            for (int i = 0; i < panel.getNumDocuments(); ++i)
                if (auto* doc = panel.getDocument (i); doc != panel.getActiveDocument())
                    pendingClick = doc->getName();

            auto* root = rootItem();
            auto* item = root != nullptr ? findItem (*root, pendingClick) : nullptr;
            if (item == nullptr)
                return;

            for (auto* parent = item->getParentItem(); parent != nullptr; parent = parent->getParentItem())
                parent->setOpen (true);
            tree()->scrollToKeepItemVisible (item);
        }

        void clickPreparedItem()
        {
            auto* root = rootItem();
            auto* item = root != nullptr ? findItem (*root, pendingClick) : nullptr;
            auto* itemComponent = item != nullptr ? tree()->getItemComponent (item) : nullptr;
            if (itemComponent == nullptr)
                return;

            const juce::Point<float> position { 200.0f, 5.0f };
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                position,
                {},
                juce::MouseInputSource::defaultPressure,
                juce::MouseInputSource::defaultOrientation,
                juce::MouseInputSource::defaultRotation,
                juce::MouseInputSource::defaultTiltX,
                juce::MouseInputSource::defaultTiltY,
                itemComponent->getParentComponent(),
                itemComponent->getParentComponent(),
                now,
                position,
                now,
                1,
                false);
            item->itemClicked (event);
            ++clicks;
        }

        juce::String verify()
        {
            auto* root = rootItem();
            if (root == nullptr)
                return "no tree";

            if (clicks > 0 && step % steps.size() == 5 && (panel.getActiveDocument() == nullptr || panel.getActiveDocument()->getName() != pendingClick))
                return "clicking " + pendingClick + " in the tree didn't activate its tab";

            auto* panelItem = findItem (*root, "Panel");
            if (panelItem == nullptr)
                return "no Panel item";

            if (auto* tabs = panel.getCurrentTabbedComponent())
            {
                if (panelItem->getNumSubItems() != 1)
                    return "Panel should show only its TabbedComponent, shows " + juce::String (panelItem->getNumSubItems());

                auto* tabsItem = subItem (*panelItem, 0);
                if (tabsItem == nullptr || tabsItem->getNumSubItems() != tabs->getNumTabs())
                    return "TabbedComponent should show " + juce::String (tabs->getNumTabs()) + " tabs, shows " + juce::String (tabsItem != nullptr ? tabsItem->getNumSubItems() : -1);

                for (int i = 0; i < tabs->getNumTabs(); ++i)
                    if (auto* tabItem = subItem (*tabsItem, i); tabItem == nullptr || tabItem->getComponentName() != tabs->getTabContentComponent (i)->getName())
                        return "tab " + juce::String (i) + " item doesn't match its content";

                ++tabChecks;
                return {};
            }

            if (panelItem->getNumSubItems() != panel.getNumChildComponents())
                return "untabbed Panel should show its " + juce::String (panel.getNumChildComponents()) + " children, shows " + juce::String (panelItem->getNumSubItems());

            ++rawChecks;
            return {};
        }
    };

    class App final : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override { return "MdpTreeTest"; }
        const juce::String getApplicationVersion() override { return "1.0"; }

        void initialise (const juce::String&) override
        {
            window = std::make_unique<juce::DocumentWindow> ("MdpTreeTest", juce::Colours::darkgrey, juce::DocumentWindow::allButtons);
            auto* test = new Test ([this] (bool ok) {
                std::fprintf (stderr, "%s\n", ok ? "PASS" : "FAIL");
                std::fflush (stderr);
                setApplicationReturnValue (ok ? 0 : 1);
                juce::MessageManager::callAsync ([] { quit(); });
            });
            window->setContentOwned (test, true);
            window->setVisible (true);
            test->start();
        }

        void shutdown() override { window.reset(); }

    private:
        std::unique_ptr<juce::DocumentWindow> window;
    };
}

START_JUCE_APPLICATION (App)
