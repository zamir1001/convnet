﻿using Convnet.Common;
using Convnet.Properties;
using dnncore;
using ICSharpCode.AvalonEdit.Document;
using Microsoft.Build.Evaluation;
using Microsoft.Build.Execution;
using Microsoft.Build.Framework;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Threading;

namespace Convnet.PageViewModels
{
    public class EditPageViewModel : PageViewModelBase
    {
#if DEBUG
        const string Mode = "Debug";
#else
        const string Mode = "Release";
#endif
        private static DispatcherTimer clickWaitTimer = new DispatcherTimer(new TimeSpan(0, 0, 0, 1), DispatcherPriority.Background, mouseWaitTimer_Tick, Dispatcher.CurrentDispatcher);
        private static Action ScriptDialogAction { get; set; }

        public event EventHandler Open;
        public event EventHandler Save;

        private string modelName = Settings.Default.ModelNameActive;
        private string definition = Settings.Default.DefinitionEditing;
        private bool definitionStatus = false;
        private bool canSynchronize = false;
        private int selectionStart = 0;
        private int selectionLength = 0;
        private TextLocation textLocation = new TextLocation(1, 1);
        private string filePath;
        private bool wordWrap = false;
        private bool showLineNumbers = true;
        private string script = File.ReadAllText(ScriptsDirectory + @"ScriptsDialog\ScriptCatalog.cs");
        private string parameters = File.ReadAllText(ScriptsDirectory + @"ScriptsDialog\ScriptParameters.cs");
        private bool dirty = true;
        private bool initAction = true;
       
        public EditPageViewModel(Model model) : base(model)
        {
            ScriptDialogAction = new Action(() => ScriptDialog());

            AddCommandButtons();
        }

        private void AddCommandButtons()
        {
            Button openButton = new Button
            {
                Name = "ButtonOpen",
                ToolTip = "Open",
                Content = new BitmapToImage(Resources.Open),
                ClickMode = ClickMode.Release
            };
            openButton.Click += new RoutedEventHandler(OpenButtonClick);

            Button saveButton = new Button
            {
                Name = "ButtonSave",
                ToolTip = "Save",
                Content = new BitmapToImage(Resources.Save),
                ClickMode = ClickMode.Release
            };
            saveButton.Click += new RoutedEventHandler(SaveButtonClick);

            Button checkButton = new Button
            {
                Name = "ButtonCheck",
                ToolTip = "Check",
                Content = new BitmapToImage(Resources.SpellCheck),
                ClickMode = ClickMode.Release
            };
            checkButton.Click += new RoutedEventHandler(CheckButtonClick);

            Button synchronizeButton = new Button
            {
                Name = "ButtonSynchronize",
                ToolTip = "Synchronize",
                Content = new BitmapToImage(Resources.Synchronize),
                ClickMode = ClickMode.Release
            };
            synchronizeButton.Click += new RoutedEventHandler(SynchronizeButtonClick);
            Binding binding = new Binding("CanSynchronize")
            {
                Converter = new Converters.BooleanToVisibilityConverter(),
                Source = this
            };
            BindingOperations.SetBinding(synchronizeButton, Button.VisibilityProperty, binding);

            var scriptsButton = new Button
            {
                Name = "ButtonScripts",
                ToolTip = "Run Script",
                Content = new BitmapToImage(Resources.Calculate),
                ClickMode = ClickMode.Release,
            };
            scriptsButton.Click += new RoutedEventHandler(ScriptsButtonClick);
            scriptsButton.MouseDoubleClick += ScriptsButtonMouseDoubleClick;

            CommandToolBar.Add(openButton);
            CommandToolBar.Add(saveButton);
            CommandToolBar.Add(checkButton);
            CommandToolBar.Add(synchronizeButton);
            CommandToolBar.Add(scriptsButton);
        }

        public override string DisplayName => "Edit";

        public override void Reset()
        {
            DefinitionStatus = false;
        }

        public string Definition
        {
            get { return definition; }
            set
            {
                if (definition == value)
                    return;

                definition = value;
                OnPropertyChanged(nameof(Definition));
                Settings.Default.DefinitionEditing = definition;
                Settings.Default.Save();

                ModelName = definition.Split(Environment.NewLine.ToCharArray(), StringSplitOptions.RemoveEmptyEntries)[0].Trim().Replace("[", "").Replace("]", "").Trim();
                DefinitionStatus = false;
            }
        }

        public string FilePath
        {
            get { return filePath; }
            set
            {
                if (filePath == value)
                    return;

                filePath = value;
                OnPropertyChanged(nameof(FilePath));
            }
        }

        public bool WordWrap
        {
            get { return wordWrap; }
            set
            {
                if (wordWrap == value)
                    return;

                wordWrap = value;
                OnPropertyChanged(nameof(WordWrap));
            }
        }

        public bool ShowLineNumbers
        {
            get { return showLineNumbers; }
            set
            {
                if (showLineNumbers == value)
                    return;

                showLineNumbers = value;
                OnPropertyChanged(nameof(ShowLineNumbers));
            }
        }

        public int SelectionStart
        {
            get { return selectionStart; }
            set
            {
                selectionStart = value;
                OnPropertyChanged(nameof(SelectionStart));
            }
        }

        public int SelectionLength
        {
            get { return selectionLength; }
            set
            {
                selectionLength = value;
                OnPropertyChanged(nameof(SelectionLength));
            }
        }

        public TextLocation TextLocation
        {
            get { return textLocation; }
            set
            {
                if (textLocation == value)
                    return;

                textLocation = value;
                OnPropertyChanged(nameof(TextLocation));
            }
        }

        public bool DefinitionStatus
        {
            get { return definitionStatus; }

            set
            {
                if (value == definitionStatus)
                    return;

                definitionStatus = value;
                OnPropertyChanged(nameof(DefinitionStatus));

                string editing = Definition.ToLower();
                string active = Settings.Default.DefinitionActive.ToLower();
                CanSynchronize = definitionStatus && Model != null && Model.TaskState == DNNTaskStates.Stopped && !editing.Equals(active);
            }
        }

        public bool CanSynchronize
        {
            get { return canSynchronize; }

            set
            {
                if (value == canSynchronize)
                    return;

                canSynchronize = value;
                OnPropertyChanged(nameof(CanSynchronize));
            }
        }

        public string ModelName
        {
            get { return modelName; }
            set
            {
                if (value.Equals(modelName))
                    return;

                if (value.Trim().All(c => char.IsLetterOrDigit(c) || c == '-'))
                {
                    modelName = value.Trim();
                    OnPropertyChanged(nameof(ModelName));
                }
            }
        }

        public string Script
        {
            get { return script; }
            set
            {
                if (value == null)
                    return;

                if (value.Equals(script))
                    return;

                script = value;
                File.WriteAllText(ScriptsDirectory + @"ScriptsDialog\ScriptCatalog.cs", Script);
                dirty = true;
                OnPropertyChanged(nameof(Script));
            }
        }

        public string Parameters
        {
            get { return parameters; }
            set
            {
                if (value == null)
                    return;

                if (value.Equals(parameters))
                    return;

                parameters = value;
                File.WriteAllText(ScriptsDirectory + @"ScriptsDialog\ScriptParameters.cs", Parameters);
                dirty = true;
                OnPropertyChanged(nameof(Parameters));
            }
        }

        void OpenButtonClick(object sender, RoutedEventArgs e)
        {
            Open?.Invoke(this, EventArgs.Empty);
        }

        void SaveButtonClick(object sender, RoutedEventArgs e)
        {
            Save?.Invoke(this, EventArgs.Empty);
        }

        public void CheckButtonClick(object sender, RoutedEventArgs e)
        {
            DefinitionStatus = CheckDefinition();
        }

        public void SynchronizeButtonClick(object sender, RoutedEventArgs e)
        {
            Mouse.OverrideCursor = null;
            try
            {
                bool sameDef = Definition.ToLower().Equals(Settings.Default.DefinitionActive.ToLower());

                string pathDefinition = Path.Combine(DefinitionsDirectory, ModelName + ".definition");
                string pathStateDefinition = Path.Combine(StateDirectory, ModelName + ".definition");
                string pathWeightsDirectory = DefinitionsDirectory + ModelName + "-weights\\";
                string pathWeights = Path.Combine(pathWeightsDirectory, ModelName + ".weights");
                string pathStateWeights = Path.Combine(StateDirectory, ModelName + ".weights");


                if (!sameDef || ModelName != Model.Name)
                {
                    Mouse.OverrideCursor = Cursors.Wait;

                    MessageBoxResult overwrite = MessageBoxResult.Yes;
                    if (File.Exists(Path.Combine(DefinitionsDirectory, ModelName + ".definition")))
                    {
                        Mouse.OverrideCursor = null;
                        overwrite = Xceed.Wpf.Toolkit.MessageBox.Show("Definition already exists! Overwrite?", "File already exists", MessageBoxButton.YesNo, MessageBoxImage.None, MessageBoxResult.No);
                    }

                    if (overwrite == MessageBoxResult.Yes)
                    {
                        Mouse.OverrideCursor = Cursors.Wait;

                        File.WriteAllText(pathDefinition, Definition);
                        File.WriteAllText(pathStateDefinition, Definition);

                        if (!Directory.Exists(pathWeightsDirectory))
                            Directory.CreateDirectory(pathWeightsDirectory);

                        MessageBoxResult keepWeights = MessageBoxResult.No;
                        if (sameDef)
                        {
                            Mouse.OverrideCursor = null;
                            keepWeights = Xceed.Wpf.Toolkit.MessageBox.Show("Keep model weights?", "Same Model", MessageBoxButton.YesNo, MessageBoxImage.None, MessageBoxResult.Yes);

                            if (keepWeights == MessageBoxResult.Yes)
                                Model.SaveWeights(pathWeights, Settings.Default.PersistOptimizer);
                        }

                        try
                        {
                            Model.Dispose();
                            Model = new Model(ModelName, pathDefinition, (DNNOptimizers)Settings.Default.Optimizer);
                            Model.SetPersistOptimizer(Settings.Default.PersistOptimizer);
                            Model.SetDisableLocking(Settings.Default.DisableLocking);
                            Model.SetOptimizersHyperParameters(Settings.Default.AdaDeltaEps, Settings.Default.AdaGradEps, Settings.Default.AdamEps, Settings.Default.AdamBeta2, Settings.Default.AdamaxEps, Settings.Default.AdamaxBeta2, Settings.Default.RMSpropEps, Settings.Default.RAdamEps, Settings.Default.RAdamBeta1, Settings.Default.RAdamBeta2);
                            Settings.Default.Save();
                            Model.BlockSize = (UInt64)Settings.Default.PixelSize;

                            if (keepWeights == MessageBoxResult.Yes)
                                Model.LoadWeights(pathWeights, Settings.Default.PersistOptimizer);

                            Settings.Default.ModelNameActive = Model.Name;
                            Settings.Default.DefinitionEditing = Definition;
                            Settings.Default.DefinitionActive = Definition;
                            Settings.Default.Save();

                            Application.Current.MainWindow.Title = Model.Name + " - Convnet Explorer";
                            CanSynchronize = false;

                            GC.Collect(GC.MaxGeneration);
                            GC.WaitForFullGCComplete();

                            Mouse.OverrideCursor = null;
                            Xceed.Wpf.Toolkit.MessageBox.Show("Model synchronized", "Information", MessageBoxButton.OK);
                        }
                        catch (Exception ex)
                        {
                            Mouse.OverrideCursor = null;
                            Xceed.Wpf.Toolkit.MessageBox.Show("An error occured during synchronization:\r\n" + ex.ToString(), "Synchronize Debug Information", MessageBoxButton.OK);
                        }
                    }
                }
                else
                {
                    if (ModelName != Model.Name)
                    {
                        Mouse.OverrideCursor = Cursors.Wait;

                        MessageBoxResult overwrite = MessageBoxResult.Yes;
                        if (File.Exists(Path.Combine(DefinitionsDirectory, ModelName + ".definition")))
                        {
                            Mouse.OverrideCursor = null;
                            overwrite = Xceed.Wpf.Toolkit.MessageBox.Show("Definition already exists! Overwrite?", "File already exists", MessageBoxButton.YesNo, MessageBoxImage.None, MessageBoxResult.No);
                        }

                        if (overwrite == MessageBoxResult.Yes)
                        {
                            Mouse.OverrideCursor = Cursors.Wait;

                            File.WriteAllText(pathDefinition, Definition);
                            File.WriteAllText(pathStateDefinition, Definition);

                            if (!Directory.Exists(pathWeightsDirectory))
                                Directory.CreateDirectory(pathWeightsDirectory);

                            MessageBoxResult keepWeights = MessageBoxResult.No;

                            Mouse.OverrideCursor = null;
                            keepWeights = Xceed.Wpf.Toolkit.MessageBox.Show("Keep model weights?", "Same Network", MessageBoxButton.YesNo, MessageBoxImage.None, MessageBoxResult.Yes);

                            if (keepWeights == MessageBoxResult.Yes)
                                Model.SaveWeights(pathWeights, Settings.Default.PersistOptimizer);

                            try
                            {
                                Model.Dispose();
                                Model = new Model(ModelName, pathDefinition, (DNNOptimizers)Settings.Default.Optimizer);
                                Model.SetPersistOptimizer(Settings.Default.PersistOptimizer);
                                Model.SetDisableLocking(Settings.Default.DisableLocking);
                                Model.SetOptimizersHyperParameters(Settings.Default.AdaDeltaEps, Settings.Default.AdaGradEps, Settings.Default.AdamEps, Settings.Default.AdamBeta2, Settings.Default.AdamaxEps, Settings.Default.AdamaxBeta2, Settings.Default.RMSpropEps, Settings.Default.RAdamEps, Settings.Default.RAdamBeta1, Settings.Default.RAdamBeta2);
                                Settings.Default.Save();
                                Model.BlockSize = (UInt64)Settings.Default.PixelSize;

                                if (keepWeights == MessageBoxResult.Yes)
                                    Model.LoadWeights(pathWeights, Settings.Default.PersistOptimizer);

                                Settings.Default.ModelNameActive = Model.Name;
                                Settings.Default.DefinitionEditing = Definition;
                                Settings.Default.DefinitionActive = Definition;
                                Settings.Default.Save();

                                Application.Current.MainWindow.Title = Model.Name + " - Convnet Explorer";
                                CanSynchronize = false;

                                GC.Collect(GC.MaxGeneration);
                                GC.WaitForFullGCComplete();

                                Mouse.OverrideCursor = null;
                                Xceed.Wpf.Toolkit.MessageBox.Show("Model synchronized", "Information", MessageBoxButton.OK);
                            }
                            catch (Exception ex)
                            {
                                Mouse.OverrideCursor = null;
                                Xceed.Wpf.Toolkit.MessageBox.Show("An error occured during synchronization:\r\n" + ex.ToString(), "Synchronize Debug Information", MessageBoxButton.OK);
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Mouse.OverrideCursor = null;
                Xceed.Wpf.Toolkit.MessageBox.Show("An error occured during synchronization:\r\n" + ex.ToString(), "Synchronize Debug Information", MessageBoxButton.OK);
            }
        }

        private static void mouseWaitTimer_Tick(object sender, EventArgs e)
        {
            clickWaitTimer.Stop();
            Dispatcher.CurrentDispatcher.Invoke(ScriptDialogAction);
        }

        void ScriptsButtonClick(object sender, RoutedEventArgs e)
        {
            clickWaitTimer.Start();
            initAction = false;
        }

        private void ScriptsButtonMouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            clickWaitTimer.Stop();
            e.Handled = true;
        }

        private async Task ScriptsDialogAsync()
        {
            var results = await ProcessAsyncHelper.RunAsync(new ProcessStartInfo(ScriptPath + @"ScriptsDialog.exe"), null);
            var fileInfo = new FileInfo(ScriptPath + @"script.txt");

            if (fileInfo.Exists)
            {
                var security = new FileSecurity(fileInfo.FullName, AccessControlSections.Owner | AccessControlSections.Group | AccessControlSections.Access);
                var authorizationRules = security.GetAccessRules(true, true, typeof(NTAccount));
                var owner = security.GetOwner(typeof(NTAccount));
                security.ModifyAccessRule(AccessControlModification.Add, new FileSystemAccessRule(owner, FileSystemRights.Modify, AccessControlType.Allow), out bool modified);

                Definition = File.ReadAllText(ScriptPath + @"script.txt");
                ModelName = Definition.Split(new string[] { "\r\n" }, StringSplitOptions.RemoveEmptyEntries)[0].Replace("[", "").Replace("]", "");

                fileInfo.Delete();
            }
        }

        private void ScriptDialog()
        {
            if (!initAction)
            {
                if (dirty)
                {
                    Mouse.OverrideCursor = Cursors.Wait;
                    IsValid = false;

                    var projectFilePath = ScriptsDirectory + @"ScriptsDialog\ScriptsDialog.csproj";
                    Dictionary<string, string> GlobalProperty = new Dictionary<string, string>
                    {
                        { "Configuration", Mode },
                        { "Platform", "AnyCPU" },
                    };
                    ProjectCollection pc = new ProjectCollection(GlobalProperty, null, ToolsetDefinitionLocations.Default);
                    BuildParameters bp = new BuildParameters(pc)
                    {
                        OnlyLogCriticalEvents = true,
                        DetailedSummary = true,
                        MaxNodeCount = 1
                    };

                    var tempFilePath = Path.GetTempFileName();
                    var fileInfo = new FileInfo(tempFilePath)
                    {
                        Attributes = FileAttributes.Temporary
                    };
                    bp.Loggers = new List<ILogger>() { new BasicFileLogger() { Parameters = fileInfo.FullName } };
                    bp.Loggers.FirstOrDefault().Verbosity = LoggerVerbosity.Diagnostic;

                    BuildRequestData buildRequest = new BuildRequestData(projectFilePath, GlobalProperty, null, new string[] { "Build" }, null);
                    BuildResult buildResult = BuildManager.DefaultBuildManager.Build(bp, buildRequest);
                    BuildManager.DefaultBuildManager.ResetCaches();
                    BuildManager.DefaultBuildManager.ShutdownAllNodes();

                    pc.UnloadAllProjects();
                    pc.UnregisterAllLoggers();
                    pc.Dispose();
                    BuildManager.DefaultBuildManager.Dispose();

                    Mouse.OverrideCursor = null;
                    IsValid = true;

                    if (buildResult.OverallResult == BuildResultCode.Success)
                        dirty = false;
                    else
                    {
                        Xceed.Wpf.Toolkit.MessageBox.Show(File.ReadAllText(fileInfo.FullName), "Compiler Result", MessageBoxButton.OK);
                        fileInfo.Delete();

                        /*
                        Mouse.OverrideCursor = Cursors.Wait;
                        IsValid = false;

                        if (dirty)
                        {
                            var builder = new MsBuilder(projectFilePath, Mode, "AnyCPU");
                            var success = builder.Build(out string buildOutput);

                            Mouse.OverrideCursor = null;
                            IsValid = true;

                            if (success)
                                dirty = false;
                            else
                                Xceed.Wpf.Toolkit.MessageBox.Show(buildOutput, "Compiler Result", MessageBoxButton.OK);
                        }
                        */
                    }
                }

                try
                {
                    if (!dirty)
                    {
                        var task = ScriptsDialogAsync();
                    }
                }
                catch (Exception exception)
                {
                    Xceed.Wpf.Toolkit.MessageBox.Show(exception.Message, "Load Assembly", MessageBoxButton.OK);
                }
            }
        }

        bool CheckDefinition()
        {
            DNNCheckMsg msg = Model.CheckDefinition(Definition);

            Definition = msg.Definition;

            if (msg.Error)
            {
                TextLocation = new TextLocation((int)msg.Row - 1, (int)msg.Column);
                TextLocation = new TextLocation((int)msg.Row, (int)msg.Column);
                Xceed.Wpf.Toolkit.MessageBox.Show(msg.Message, "Check Information", MessageBoxButton.OK);
            }

            return !msg.Error;
        }
    }
}