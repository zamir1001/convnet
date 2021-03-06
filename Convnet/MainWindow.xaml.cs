﻿using Convnet.Common;
using Convnet.Dialogs;
using Convnet.PageViewModels;
using Convnet.Properties;
using CsvHelper;
using dnncore;
using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;


namespace Convnet
{
    public partial class MainWindow : Window, IDisposable
    {
        const string Framework = "netcoreapp3.1";
#if DEBUG
        const string Mode = "Debug";
#else
        const string Mode = "Release";
#endif
        public static string ApplicationPath { get; } = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) + @"\";
        public static string StorageDirectory { get; } = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments) + @"\convnet\";
        public static string StateDirectory { get; } = StorageDirectory + @"state\";
        public static string DefinitionsDirectory { get; } = StorageDirectory + @"definitions\";
        public static string ScriptsDirectory { get; } = StorageDirectory + @"scripts\";

        public static RoutedUICommand AboutCmd = new RoutedUICommand();
        public static RoutedUICommand DisableLockingCmd = new RoutedUICommand();
        public static RoutedUICommand PlainFormatCmd = new RoutedUICommand();
        public static RoutedUICommand LockAllCmd = new RoutedUICommand();
        public static RoutedUICommand UnlockAllCmd = new RoutedUICommand();
        public static RoutedUICommand PersistOptimizerCmd = new RoutedUICommand();

        public bool ShowCloseApplicationDialog = true;
        public PageViewModel PageVM;

        public static void Copy(string sourceDirectory, string targetDirectory)
        {
            var diSource = new DirectoryInfo(sourceDirectory);
            var diTarget = new DirectoryInfo(targetDirectory);

            CopyAll(diSource, diTarget);
        }

        public static void CopyAll(DirectoryInfo source, DirectoryInfo target)
        {
            Directory.CreateDirectory(target.FullName);

            // Copy each file into the new directory.
            foreach (var fi in source.GetFiles())
            {
                //Console.WriteLine(@"Copying {0}\{1}", target.FullName, fi.Name);
                fi.CopyTo(Path.Combine(target.FullName, fi.Name), true);
            }

            // Copy each subdirectory using recursion.
            foreach (var diSourceSubDir in source.GetDirectories())
            {
                DirectoryInfo nextTargetSubDir = target.CreateSubdirectory(diSourceSubDir.Name);
                CopyAll(diSourceSubDir, nextTargetSubDir);
            }
        }

        public MainWindow()
        {
            InitializeComponent();

            Directory.CreateDirectory(StorageDirectory);
            Directory.CreateDirectory(DefinitionsDirectory);
            Directory.CreateDirectory(StateDirectory);

            if (!Directory.Exists(ScriptsDirectory))
            {
                Directory.CreateDirectory(ScriptsDirectory);
                Copy(ApplicationPath.Replace(@"Convnet\bin\x64\" + Mode + @"\" + Framework + @"\", "") + @"ScriptsDialog\", ScriptsDirectory);
            }

            if (!File.Exists(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + ".txt")))
            {
                Directory.CreateDirectory(DefinitionsDirectory + @"resnet-32-3-2-6-dropout-channelzeropad-relu-weights\");
                Directory.CreateDirectory(DefinitionsDirectory + Settings.Default.ModelNameActive + @"-weights\");
                File.Copy(ApplicationPath + @"Resources\state\resnet-32-3-2-6-dropout-channelzeropad-relu.txt", StateDirectory + "resnet-32-3-2-6-dropout-channelzeropad-relu.txt", true);
                File.Copy(ApplicationPath + @"Resources\state\resnet-32-3-2-6-dropout-channelzeropad-relu.txt", DefinitionsDirectory + "resnet-32-3-2-6-dropout-channelzeropad-relu.txt", true);

                Settings.Default.ModelNameActive = "resnet-32-3-2-6-dropout-channelzeropad-relu";
                Settings.Default.Optimizer = DNNOptimizers.NAG;
                Settings.Default.Save();
            }

            try
            {
                Model model = new Model(Settings.Default.ModelNameActive, Path.Combine(StateDirectory, Settings.Default.ModelNameActive + ".txt"));
                if (model != null)
                {
                    PageVM = new PageViewModel(model);
                    if (PageVM != null)
                    {
                        PageVM.Model.SetFormat(Settings.Default.PlainFormat);
                        PageVM.Model.SetOptimizer(Settings.Default.Optimizer);
                        PageVM.Model.SetPersistOptimizer(Settings.Default.PersistOptimizer);
                        PageVM.Model.SetDisableLocking(Settings.Default.DisableLocking);
                        PageVM.Model.BlockSize = (ulong)Settings.Default.PixelSize;

                        if (Settings.Default.PersistOptimizer)
                        {
                            if (File.Exists(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + "-(" + PageVM.Model.Optimizer.ToString().ToLower(CultureInfo.CurrentCulture) + @").bin")))
                            {
                                if (PageVM.Model.LoadWeights(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + "-(" + PageVM.Model.Optimizer.ToString().ToLower() + @").bin"), true) != 0)
                                {
                                    if (PageVM.Model.LoadWeights(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + @".bin"), false) == 0)
                                    {
                                        Settings.Default.PersistOptimizer = !Settings.Default.PersistOptimizer;
                                        Settings.Default.Save();
                                        PageVM.Model.SetPersistOptimizer(Settings.Default.PersistOptimizer);
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (File.Exists(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + @".bin")))
                                PageVM.Model.LoadWeights(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + @".bin"), false);
                        }
                        Settings.Default.DefinitionActive = File.ReadAllText(Path.Combine(StateDirectory, Settings.Default.ModelNameActive + ".txt"));
                        Settings.Default.Save();

                        for (int i = 0; i < Settings.Default.TrainingLog.Count; i++)
                            Settings.Default.TrainingLog[i].ElapsedTime = new TimeSpan(Settings.Default.TrainingLog[i].ElapsedTicks);
                        Settings.Default.Save();

                        Title = PageVM.Model.Name + " - Convnet Explorer";
                        DataContext = PageVM;

                        int priority = (int)Math.Round(Settings.Default.PrioritySetter);
                        switch (priority)
                        {
                            case 1:
                                PrioritySlider.ToolTip = "Low";
                                break;
                            case 2:
                                PrioritySlider.ToolTip = "Below Normal";
                                break;
                            case 3:
                                PrioritySlider.ToolTip = "Normal";
                                break;
                            case 4:
                                PrioritySlider.ToolTip = "Above Normal";
                                break;
                            case 5:
                                PrioritySlider.ToolTip = "High";
                                break;
                            case 6:
                                PrioritySlider.ToolTip = "Realtime";
                                break;
                        }
                    }
                    else
                       Xceed.Wpf.Toolkit.MessageBox.Show("Failed to create the viewmodel: " + Settings.Default.ModelNameActive, "Error", MessageBoxButton.OK);
                }
                else
                   Xceed.Wpf.Toolkit.MessageBox.Show("Failed to create the model: " + Settings.Default.ModelNameActive, "Error", MessageBoxButton.OK);
            }
            catch (Exception exception)
            {
                Xceed.Wpf.Toolkit.MessageBox.Show(exception.Message + "\r\n\r\n" + exception.GetBaseException().Message + "\r\n\r\n" + exception.InnerException.Message + "\r\n\r\nAn error occured while loading the model:" + Settings.Default.ModelNameActive, "Information", MessageBoxButton.OK);
            }
        }

        ~MainWindow()
        {
            // In case the client forgets to call
            // Dispose , destructor will be invoked for
            Dispose(false);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                // Free managed objects.

            }
            // Free unmanaged objects
        }

        public void Dispose()
        {
            Dispose(true);
            // Ensure that the destructor is not called
            GC.SuppressFinalize(this);
        }

        private void CutCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            (e.Source as TextBox).Cut();
        }

        private void CutCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (e.Source.GetType() == typeof(TextBox))
                e.CanExecute = (e.Source as TextBox).SelectionLength > 0;
            else
                e.CanExecute = false;
        }

        private void CopyCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            (e.Source as TextBox).Copy();
        }

        private void CopyCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (e.Source.GetType() == typeof(TextBox))
                e.CanExecute = (e.Source as TextBox).SelectionLength > 0;
            else
                e.CanExecute = false;
        }

        private void PasteCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            (e.Source as TextBox).Paste();
        }

        private void PasteCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = false;
        }

        private void SelectAllCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            (e.Source as TextBox).SelectAll();
        }

        private void SelectAllCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (e.Source.GetType() == typeof(TextBox))
                e.CanExecute = true;
            else
                e.CanExecute = false;
        }

        private void UndoCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            e.Handled = (e.Source as TextBox).Undo();
        }

        private void UndoCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (e.Source.GetType() == typeof(TextBox))
                e.CanExecute = (e.Source as TextBox).CanUndo;
            else
                e.CanExecute = false;
        }

        private void RedoCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            e.Handled = (e.Source as TextBox).Redo();
        }

        private void RedoCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (e.Source.GetType() == typeof(TextBox))
                e.CanExecute = (e.Source as TextBox).CanRedo;
            else
                e.CanExecute = false;
        }

        private void HelpCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            ApplicationHelper.OpenBrowser("https://github.com/zamir1001/convnet.git");
        }

        private void HelpCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = true;
        }

        private void AboutCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            Window aboutDialog = new About
            {
                Owner = this,
                WindowStartupLocation = System.Windows.WindowStartupLocation.CenterOwner
            };
            aboutDialog.ShowDialog();
        }

        private void AboutCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = true;
        }

        private void CloseCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            Close();
        }

        private void CloseCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = true;
        }

        private void OpenCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            Microsoft.Win32.OpenFileDialog openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                CheckFileExists = true,
                CheckPathExists = true,
                Multiselect = false,
                AddExtension = true,
                ValidateNames = true,
                FilterIndex = 1,
                InitialDirectory = DefinitionsDirectory
            };

            bool? ret;

            switch (PageVM.CurrentModel)
            {
                case ViewModels.Edit:
                    openFileDialog.Filter = "Definition|*.txt";
                    openFileDialog.Title = "Load Definition";
                    openFileDialog.DefaultExt = ".txt";
                    openFileDialog.FilterIndex = 1;
                    openFileDialog.InitialDirectory = DefinitionsDirectory;
                    ret = openFileDialog.ShowDialog(Application.Current.MainWindow);
                    if (ret.HasValue && ret.Value)
                    {
                        Mouse.OverrideCursor = Cursors.Wait;
                        StreamReader reader = new StreamReader(openFileDialog.FileName, true);
                        if (openFileDialog.FileName.Contains(".txt"))
                        {
                            string definition = reader.ReadToEnd().Trim();
                            (PageVM.CurrentPage as EditPageViewModel).Definition = definition;
                            Settings.Default.DefinitionEditing = definition.Trim();
                            Settings.Default.Save();
                        }
                        reader.Close();
                        reader.Dispose();

                        Mouse.OverrideCursor = null;
                        Xceed.Wpf.Toolkit.MessageBox.Show(openFileDialog.SafeFileName + " is loaded", "Information", MessageBoxButton.OK);
                    }
                    break;

                case ViewModels.Test:
                    {
                        openFileDialog.Filter = "Weights|*.bin";
                        openFileDialog.Title = "Load Weights";
                        openFileDialog.DefaultExt = ".bin";
                        openFileDialog.FilterIndex = 1;
                        openFileDialog.InitialDirectory = DefinitionsDirectory + PageVM.Model.Name + @"-weights\";
                        ret = openFileDialog.ShowDialog(Application.Current.MainWindow);
                        if (ret.HasValue && ret.Value)
                        {
                            Mouse.OverrideCursor = Cursors.Wait;
                            if (openFileDialog.FileName.EndsWith(".bin"))
                            {
                                if (PageVM.Model.LoadWeights(openFileDialog.FileName, Settings.Default.PersistOptimizer) == 0)
                                {
                                    var tpvm = PageVM.Pages[(int)ViewModels.Train] as TrainPageViewModel;
                                    tpvm.Optimizer = tpvm.Model.Optimizer;
                                    tpvm.RefreshButtonClick(this, null);

                                    Mouse.OverrideCursor = null;
                                    Xceed.Wpf.Toolkit.MessageBox.Show(openFileDialog.SafeFileName + " is loaded", "Information", MessageBoxButton.OK);
                                }
                                else
                                {
                                    Mouse.OverrideCursor = null;
                                    Xceed.Wpf.Toolkit.MessageBox.Show(openFileDialog.SafeFileName + " is incompatible", "Information", MessageBoxButton.OK);
                                }
                            }
                            Mouse.OverrideCursor = null;
                        }
                    }
                    break;

                case ViewModels.Train:
                    if (PageVM.CurrentPage.Model.TaskState == DNNTaskStates.Stopped)
                    {
                        openFileDialog.Filter = "Weights|*.bin|Log|*.csv";
                        openFileDialog.Title = "Load Weights";
                        openFileDialog.DefaultExt = ".bin";
                    }
                    else
                    {
                        openFileDialog.Filter = "Log|*.csv";
                        openFileDialog.Title = "Load Log";
                        openFileDialog.DefaultExt = ".cvs";
                    }

                    openFileDialog.FilterIndex = 1;
                    openFileDialog.InitialDirectory = DefinitionsDirectory + PageVM.Model.Name + @"-weights\";
                    ret = openFileDialog.ShowDialog(Application.Current.MainWindow);
                    if (ret.HasValue && ret.Value)
                    {
                        Mouse.OverrideCursor = Cursors.Wait;
                        if (openFileDialog.FileName.EndsWith(".bin"))
                        {
                            if (PageVM.Model.LoadWeights(openFileDialog.FileName, Settings.Default.PersistOptimizer) == 0)
                            {
                                var tpvm = PageVM.Pages[(int)ViewModels.Train] as TrainPageViewModel;
                                tpvm.Optimizer = tpvm.Model.Optimizer;
                                tpvm.RefreshButtonClick(this, null);
                                                                
                                Mouse.OverrideCursor = null;
                                Xceed.Wpf.Toolkit.MessageBox.Show(openFileDialog.SafeFileName + " is loaded", "Information", MessageBoxButton.OK);
                            }
                            else
                            {
                                Mouse.OverrideCursor = null;
                                Xceed.Wpf.Toolkit.MessageBox.Show(openFileDialog.SafeFileName + " is incompatible", "Information", MessageBoxButton.OK);
                            }
                        }
                        else if (openFileDialog.FileName.EndsWith(".csv"))
                        {
                            if (PageVM.CurrentPage is TrainPageViewModel tpvm)
                            {
                                ObservableCollection<DNNTrainingResult> backup = new ObservableCollection<DNNTrainingResult>(Settings.Default.TrainingLog);
                               
                                try
                                {
                                    CsvHelper.Configuration.CsvConfiguration config = new CsvHelper.Configuration.CsvConfiguration(CultureInfo.CurrentCulture)
                                    {
                                        HasHeaderRecord = true,
                                        DetectDelimiter = true,
                                        DetectDelimiterValues = new string[] { ";" },
                                        Delimiter = ";"
                                    };

                                    using (var reader = new StreamReader(openFileDialog.FileName, true))
                                    using (var csv = new CsvReader(reader, config))
                                    {
                                        var records = csv.GetRecords<DNNTrainingResult>();

                                        if (Settings.Default.TrainingLog.Count > 0)
                                        {
                                            Mouse.OverrideCursor = null;
                                            if (Xceed.Wpf.Toolkit.MessageBox.Show("Do you want to clear the existing log?", "Clear Log", MessageBoxButton.YesNo, MessageBoxImage.None, MessageBoxResult.No) == MessageBoxResult.Yes)
                                                Settings.Default.TrainingLog.Clear();
                                        }

                                        foreach (var record in records)
                                            Settings.Default.TrainingLog.Add(record);
                                        
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Settings.Default.TrainingLog = backup;
                                    Mouse.OverrideCursor = null;
                                    Xceed.Wpf.Toolkit.MessageBox.Show(ex.Message);
                                }
                               
                                Settings.Default.Save();

                                tpvm.RefreshTrainingPlot();

                                Mouse.OverrideCursor = null;
                                Xceed.Wpf.Toolkit.MessageBox.Show(openFileDialog.SafeFileName + " is loaded", "Information", MessageBoxButton.OK);
                            }
                        }
                        Mouse.OverrideCursor = null;
                    }
                    break;
            }
        }

        private void SaveCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            var fileName = PageVM.Model.Name + (Settings.Default.PersistOptimizer ? "-(" + PageVM.Model.Optimizer.ToString().ToLower() + ").bin" : ".bin");
            var directory = DefinitionsDirectory + PageVM.Model.Name + @"-weights\";
            if (File.Exists(directory + fileName))
            {
                if (Xceed.Wpf.Toolkit.MessageBox.Show("Do you want to overwrite the existing file?", "File already exists", MessageBoxButton.YesNo, MessageBoxImage.None, MessageBoxResult.No) == MessageBoxResult.Yes)
                {
                    Mouse.OverrideCursor = Cursors.Wait;
                    SaveWeights();
                }
            }
            else
            {
                Mouse.OverrideCursor = Cursors.Wait;
                SaveWeights();
            }
            Mouse.OverrideCursor = null;
        }

        private void SaveWeights()
        {
            Mouse.OverrideCursor = Cursors.Wait;
            if (Settings.Default.PersistOptimizer)
            {
                var fileName = PageVM.Model.Name + "-(" + PageVM.Model.Optimizer.ToString().ToLower() + ").bin";
                var directory = DefinitionsDirectory + PageVM.Model.Name + @"-weights\";
                PageVM.Model.SaveWeights(directory + fileName, true);
                if (File.Exists(directory + fileName))
                {
                    File.Copy(directory + fileName, StateDirectory + fileName, true);
                    Mouse.OverrideCursor = null;
                    Xceed.Wpf.Toolkit.MessageBox.Show("Weights are saved", "Information", MessageBoxButton.OK);
                }
                else
                    Mouse.OverrideCursor = null;
            }
            else
            {
                var fileName = PageVM.Model.Name + ".bin";
                var directory = DefinitionsDirectory + PageVM.Model.Name + @"-weights\";
                PageVM.Model.SaveWeights(directory + fileName, false);
                if (File.Exists(directory + fileName))
                {
                    File.Copy(directory + fileName, StateDirectory + fileName, true);
                    Mouse.OverrideCursor = null;
                    Xceed.Wpf.Toolkit.MessageBox.Show("Weights are saved", "Information", MessageBoxButton.OK);
                }
                else
                    Mouse.OverrideCursor = null;
            }
        }

        private void SaveAsCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            Microsoft.Win32.SaveFileDialog saveFileDialog = new Microsoft.Win32.SaveFileDialog
            {
                FileName = PageVM.Model.Name,
                AddExtension = true,
                CreatePrompt = false,
                OverwritePrompt = true,
                ValidateNames = true,
                Title = "Save Model",
                InitialDirectory = DefinitionsDirectory
            };

            bool? ret;

            switch (PageVM.CurrentModel)
            {
                case ViewModels.Edit:
                    saveFileDialog.FileName = (PageVM.CurrentPage as EditPageViewModel).ModelName;
                    saveFileDialog.Filter = "Definition|*.txt";
                    saveFileDialog.Title = "Save Model";
                    saveFileDialog.DefaultExt = ".txt";
                    saveFileDialog.FilterIndex = 1;
                    saveFileDialog.InitialDirectory = DefinitionsDirectory;
                    ret = saveFileDialog.ShowDialog(Application.Current.MainWindow);
                    if (ret.HasValue && ret.Value)
                    {
                        Mouse.OverrideCursor = Cursors.Wait;
                        TextWriter writer = new StreamWriter(saveFileDialog.FileName, false);
                        if (saveFileDialog.FileName.Contains(".txt"))
                            writer.Write(Settings.Default.DefinitionEditing);
                        writer.Flush();
                        writer.Close();
                        writer.Dispose();
                        Mouse.OverrideCursor = null;
                        Xceed.Wpf.Toolkit.MessageBox.Show(saveFileDialog.SafeFileName + " saved", "Information", MessageBoxButton.OK);
                    }
                    break;

                case ViewModels.Train:
                    saveFileDialog.FileName = Settings.Default.PersistOptimizer ? (PageVM.CurrentPage as TrainPageViewModel).Model.Name + @"-(" + (PageVM.CurrentPage as TrainPageViewModel).Optimizer.ToString().ToLower() + @")": (PageVM.CurrentPage as TrainPageViewModel).Model.Name;
                    saveFileDialog.Filter = "Weights|*.bin|Log|*.csv";
                    saveFileDialog.Title = "Save";
                    saveFileDialog.DefaultExt = ".bin";
                    saveFileDialog.FilterIndex = 1;
                    saveFileDialog.InitialDirectory = DefinitionsDirectory + PageVM.Model.Name + @"-weights\";
                    ret = saveFileDialog.ShowDialog(Application.Current.MainWindow);
                    if (ret.HasValue && ret.Value)
                    {
                        if (saveFileDialog.FileName.EndsWith(".csv"))
                        {
                            if (PageVM.CurrentPage is TrainPageViewModel tpvm)
                            {
                                try
                                {
                                    const string delim = ";";
                                    var sb = new StringBuilder();
                                    sb.AppendLine(
                                            "Cycle" + delim +
                                            "Epoch" + delim +
                                            "GroupIndex" + delim +
                                            "CostIndex" + delim +
                                            "CostName" + delim +
                                            "Optimizer" + delim +
                                            "Momentum" + delim +
                                            "Beta2" + delim +
                                            "L2Penalty" + delim +
                                            "Eps" + delim +
                                            "Rate" + delim +
                                            "BatchSize" + delim +
                                            "Dropout" + delim +
                                            "Cutout" + delim +
                                            "AutoAugment" + delim +
                                            "HorizontalFlip" + delim +
                                            "VerticalFlip" + delim +
                                            "ColorCast" + delim +
                                            "ColorAngle" + delim +
                                            "Distortion" + delim +
                                            "Interpolation" + delim +
                                            "Scaling" + delim +
                                            "Rotation" + delim +
                                            "AvgTrainLoss" + delim +
                                            "TrainErrors" + delim +
                                            "TrainErrorPercentage" + delim +
                                            "TrainAccuracy" + delim +
                                            "AvgTestLoss" + delim +
                                            "TestErrors" + delim +
                                            "TestErrorPercentage" + delim +
                                            "TestAccuracy" + delim +
                                            "ElapsedTicks" + delim +
                                            "ElapsedTime");
                                    foreach (var row in tpvm.TrainingLog)
                                        sb.AppendLine(
                                            row.Cycle.ToString() + delim +
                                            row.Epoch.ToString() + delim +
                                            row.GroupIndex.ToString() + delim +
                                            row.CostIndex.ToString() + delim +
                                            row.CostName.ToString() + delim +
                                            row.Optimizer.ToString() + delim +
                                            row.Momentum.ToString() + delim +
                                            row.Beta2.ToString() + delim +
                                            row.L2Penalty.ToString() + delim +
                                            row.Eps.ToString() + delim +
                                            row.Rate.ToString() + delim +
                                            row.BatchSize.ToString() + delim +
                                            row.Dropout.ToString() + delim +
                                            row.Cutout.ToString() + delim +
                                            row.AutoAugment.ToString() + delim +
                                            row.HorizontalFlip.ToString() + delim +
                                            row.VerticalFlip.ToString() + delim +
                                            row.ColorCast.ToString() + delim +
                                            row.ColorAngle.ToString() + delim +
                                            row.Distortion.ToString() + delim +
                                            row.Interpolation.ToString() + delim +
                                            row.Scaling.ToString() + delim +
                                            row.Rotation.ToString() + delim +
                                            row.AvgTrainLoss.ToString() + delim +
                                            row.TrainErrors.ToString() + delim +
                                            row.TrainErrorPercentage.ToString() + delim +
                                            row.TrainAccuracy.ToString() + delim +
                                            row.AvgTestLoss.ToString() + delim +
                                            row.TestErrors.ToString() + delim +
                                            row.TestErrorPercentage.ToString() + delim +
                                            row.TestAccuracy.ToString() + delim +
                                            row.ElapsedTicks.ToString() + delim +
                                            row.ElapsedTime.ToString());

                                    //Clipboard.SetText(sb.ToString());
                                    File.WriteAllText(saveFileDialog.FileName, sb.ToString());
                                   
                                    //CsvHelper.Configuration.CsvConfiguration config = new CsvHelper.Configuration.CsvConfiguration(CultureInfo.CurrentCulture)
                                    //{
                                    //    HasHeaderRecord = true,
                                    //    DetectDelimiter = true,
                                    //    DetectDelimiterValues = new string[] { ";" },
                                    //    Delimiter = ";"
                                    //};

                                    //using (var writer = new StreamWriter(saveFileDialog.FileName))
                                    //using (var csv = new CsvWriter(writer, config))
                                    //{
                                    //    csv.WriteRecords(tpvm.TrainingLog);
                                    //}

                                    Mouse.OverrideCursor = null;
                                    Xceed.Wpf.Toolkit.MessageBox.Show(saveFileDialog.SafeFileName + " log saved", "Information", MessageBoxButton.OK);
                                }
                                catch (Exception exception)
                                {
                                    Mouse.OverrideCursor = null;
                                    Xceed.Wpf.Toolkit.MessageBox.Show(exception.ToString(), "Exception occured", MessageBoxButton.OK);
                                }
                            }
                            Mouse.OverrideCursor = null;
                        }
                        else
                        {
                            if (saveFileDialog.FileName.EndsWith(".bin"))
                            {
                                PageVM.Model.SaveWeights(saveFileDialog.FileName, Settings.Default.PersistOptimizer);
                                Mouse.OverrideCursor = null;
                                Xceed.Wpf.Toolkit.MessageBox.Show(saveFileDialog.SafeFileName + " weights saved", "Information", MessageBoxButton.OK);
                            }
                        }
                        Mouse.OverrideCursor = null;
                    }
                    break;

                case ViewModels.Test:
                    break;
            }
        }

        private void OpenCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (PageVM != null)
            {
                switch (PageVM.CurrentModel)
                {
                    case ViewModels.Edit:
                        e.CanExecute = true;
                        break;

                    case ViewModels.Test:
                        e.CanExecute = (PageVM.CurrentPage.Model.TaskState == DNNTaskStates.Stopped);
                        break;

                    case ViewModels.Train:
                        e.CanExecute = true;
                        break;
                }
            }
            else
                e.CanExecute = false;
        }

        private void SaveCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (PageVM != null)
                e.CanExecute = PageVM.CurrentModel != ViewModels.Test && PageVM.CurrentModel != ViewModels.Edit;
            else
                e.CanExecute = false;
        }

        private void SaveAsCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            if (PageVM != null)
                e.CanExecute = PageVM.CurrentModel != ViewModels.Test;
            else
                e.CanExecute = false;
        }

        private void LockAllCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            if (PageVM != null && PageVM.Model != null)
            {
                if (PageVM.CurrentModel == ViewModels.Train)
                {
                    if (PageVM.CurrentPage is TrainPageViewModel tpvm)
                    {
                        tpvm.Model.SetLocked(true);
                        Application.Current.Dispatcher.Invoke(() => tpvm.LayersComboBox_SelectionChanged(this, null), DispatcherPriority.Render);

                    }
                }
            }
        }

        private void LockAllCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = false;
            if (PageVM != null && PageVM.Model != null)
                if (PageVM.CurrentModel == ViewModels.Train)
                {
                    if (PageVM.CurrentPage is TrainPageViewModel tpvm)
                        e.CanExecute = !Settings.Default.DisableLocking;
                }
        }

        private void UnlockAllCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            if (PageVM != null && PageVM.Model != null)
            {
                if (PageVM.CurrentModel == ViewModels.Train)
                {
                    if (PageVM.CurrentPage is TrainPageViewModel tpvm)
                    {
                        tpvm.Model.SetLocked(false);
                        Application.Current.Dispatcher.Invoke(() => tpvm.LayersComboBox_SelectionChanged(this, null), DispatcherPriority.Render);
                    }
                }
            }
        }

        private void UnlockAllCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = false;
            if (PageVM != null && PageVM.Model != null)
                if (PageVM.CurrentModel == ViewModels.Train)
                {
                    if (PageVM.CurrentPage is TrainPageViewModel tpvm)
                        e.CanExecute = !Settings.Default.DisableLocking;
                }
        }

        private void PersistOptimizerCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            if (PersistOptimizer != null && PageVM != null && PageVM.Model != null)
            {
                Settings.Default.PersistOptimizer = PersistOptimizer.IsChecked;
                Settings.Default.Save();

                PageVM.Model.SetPersistOptimizer(Settings.Default.PersistOptimizer);
            }
        }

        private void PersistOptimizerCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = false;

            if (PersistOptimizer != null && PageVM != null && PageVM.Model != null)
                e.CanExecute = true;
        }

        private void DisableLockingCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            if (DisableLocking != null)
            {
                if (PageVM == null || PageVM.Model == null)
                    return;

                if (PageVM.Pages[2] is TrainPageViewModel tpvm)
                {
                    Settings.Default.DisableLocking = DisableLocking.IsChecked;
                    Settings.Default.Save();

                    tpvm.OnDisableLockingChanged(target, e);
                }
            }
        }

        private void DisableLockingCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = false;

            if (DisableLocking != null && PageVM != null && PageVM.Model != null)
                e.CanExecute = true;
        }

        private void PlainFormatCmdExecuted(object target, ExecutedRoutedEventArgs e)
        {
            if (Plain != null && PageVM != null && PageVM.Model != null)
            {
                if (PageVM.Pages[2] is TrainPageViewModel tpvm)
                {
                    if (tpvm.Model.TaskState == DNNTaskStates.Stopped)
                    {
                        if (PageVM.Model.SetFormat(Plain.IsChecked))
                        {
                            Settings.Default.PlainFormat = Plain.IsChecked;
                            Settings.Default.Save();
                        }
                    }
                }
            }
        }

        private void PlainFormatCmdCanExecute(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = false;

            if (Plain != null && PageVM != null && PageVM.Model != null)
              e.CanExecute = PageVM.Model.TaskState == DNNTaskStates.Stopped;
        }

        private void PrioritySlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (PageVM != null && PageVM.Model != null)
            {
                var temp = (int)Math.Round(Settings.Default.PrioritySetter);
                switch (temp)
                {
                    case 1:
                        Settings.Default.Priority = System.Diagnostics.ProcessPriorityClass.Idle;
                        break;
                    case 2:
                        Settings.Default.Priority = System.Diagnostics.ProcessPriorityClass.BelowNormal;
                        break;
                    case 3:
                        Settings.Default.Priority = System.Diagnostics.ProcessPriorityClass.Normal;
                        break;
                    case 4:
                        Settings.Default.Priority = System.Diagnostics.ProcessPriorityClass.AboveNormal;
                        break;
                    case 5:
                        Settings.Default.Priority = System.Diagnostics.ProcessPriorityClass.High;
                        break;
                    case 6:
                        Settings.Default.Priority = System.Diagnostics.ProcessPriorityClass.RealTime;
                        break;
                }
                PrioritySlider.ToolTip = Settings.Default.Priority.ToString();
                Process.GetCurrentProcess().PriorityClass = Settings.Default.Priority;
                Settings.Default.Save();
            }
        }
    }
}
