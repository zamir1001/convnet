﻿using Convnet.Common;
using Convnet.PageViewModels;
using dnncore;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace Convnet.Dialogs
{
    public partial class TrainParameters : Window
    {
        public DNNTrainingRate Rate { get; set; }
        public Model Model { get; set; }
        public string Path { get; set; }

        public TrainPageViewModel tpvm;

        public TrainParameters()
        {
            InitializeComponent();
        }

        void Window_Loaded(object sender, RoutedEventArgs e)
        {
            switch (Model.Dataset)
            {
                case DNNDatasets.cifar10:
                case DNNDatasets.cifar100:
                case DNNDatasets.tinyimagenet:
                    break;

                case DNNDatasets.fashionmnist:
                case DNNDatasets.mnist:
                    Rate.AutoAugment = 0.0f;
                    Rate.ColorCast = 0;
                    Rate.ColorAngle = 0;
                    textBoxAutoAugment.IsEnabled = false;
                    textBoxColorCast.IsEnabled = false;
                    textBoxColorAngle.IsEnabled = false;
                    break;
            }

            DataContext = Rate;
           
            textBoxGotoEpoch.Text = tpvm.GotoEpoch.ToString();
            CheckBoxSGDR.IsChecked = tpvm.SGDR;
            ChangeSGDR();

            textBoxColorAngle.IsEnabled = Rate.ColorCast > 0;

            comboBoOptimizer.Focus();
        }

        private bool IsValid(DependencyObject node)
        {
            // Check if dependency object was passed
            if (node != null)
            {
                // Check if dependency object is valid.
                // NOTE: Validation.GetHasError works for controls that have validation rules attached 
                bool isValid = !Validation.GetHasError(node);
                if (!isValid)
                {
                    // If the dependency object is invalid, and it can receive the focus,
                    // set the focus
                    if (node is IInputElement) Keyboard.Focus((IInputElement)node);
                    return false;
                }
            }

            // If this dependency object is valid, check all child dependency objects
            foreach (object subnode in LogicalTreeHelper.GetChildren(node))
            {
                if (subnode is DependencyObject)
                {
                    // If a child dependency object is invalid, return false immediately,
                    // otherwise keep checking
                    if (IsValid((DependencyObject)subnode) == false) return false;
                }
            }

            // All dependency objects are valid
            return true;
        }

        private void ButtonTrain_Click(object sender, RoutedEventArgs e)
        {
            if (IsValid(this))
            {
                if (Model.BatchNormalizationUsed() && Rate.BatchSize == 1)
                {
                    Xceed.Wpf.Toolkit.MessageBox.Show("Your model uses batch normalization.\r\nThe batch size cannot be equal to 1 in this case.", "Warning", MessageBoxButton.OK);
                    return;
                }
                
                uint.TryParse(textBoxGotoEpoch.Text, out uint gotoEpoch);
                if ((gotoEpoch > (tpvm.SGDR ? Rate.Epochs * Rate.Cycles * Rate.EpochMultiplier: Rate.Epochs)) || (gotoEpoch < 1))
                {
                    Xceed.Wpf.Toolkit.MessageBox.Show("Goto epoch is to large", "Warning", MessageBoxButton.OK);
                    return;
                }
                tpvm.GotoEpoch = gotoEpoch;

                Properties.Settings.Default.TrainRate = Rate;
                Properties.Settings.Default.Optimizer = Rate.Optimizer;
                Properties.Settings.Default.Save();
               
                DialogResult = true;
                Close();
            }
        }

        private void ButtonCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            buttonCancel.Focus();
        }

        private void TextBoxDistortions_TextChanged(object sender, TextChangedEventArgs e)
        {
            var enabled = (float.TryParse(textBoxDistortions.Text, out float result) && result > 0.0f);

            comboBoInterpolation.IsEnabled = enabled;
            textBoxRotation.IsEnabled = enabled;
            textBoxScaling.IsEnabled = enabled;
        }

        private void TextBoxColorCast_TextChanged(object sender, TextChangedEventArgs e)
        {
            textBoxColorAngle.IsEnabled = (float.TryParse(textBoxColorCast.Text, out float result) && result > 0.0f);
        }

        private void CheckBoxSGDR_Checked(object sender, RoutedEventArgs e)
        {
            ChangeSGDR();
        }

        private void CheckBoxSGDR_Unchecked(object sender, RoutedEventArgs e)
        {
            ChangeSGDR();
        }

        private void ChangeSGDR()
        {
            tpvm.SGDR = CheckBoxSGDR.IsChecked.HasValue ? CheckBoxSGDR.IsChecked.Value : false;
            
            textBoxCycles.IsEnabled = tpvm.SGDR;
            textBoxEpochMultiplier.IsEnabled = tpvm.SGDR;

            //textBoxDecayFactor.IsEnabled = !tpvm.SGDR;
            textBoxDecayAfterEpochs.IsEnabled = !tpvm.SGDR;
        }

        private void comboBoOptimizer_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            DNNOptimizers optimizer = (DNNOptimizers)comboBoOptimizer.SelectedIndex;

            switch (optimizer)
            {
                case DNNOptimizers.SGD:
                    {
                        textBlockMomentum.Opacity = 0.5;
                        textBoxMomentum.IsEnabled = false;
                        textBlockL2penalty.Opacity = 1;
                        textBoxL2penalty.IsEnabled = true;
                    }
                    break;

                case DNNOptimizers.AdaBound:
                case DNNOptimizers.AdaDelta:
                case DNNOptimizers.Adam:  
                case DNNOptimizers.Adamax:
                case DNNOptimizers.AmsBound:
                case DNNOptimizers.RMSProp:
                    {
                        textBlockMomentum.Opacity = 1;
                        textBoxMomentum.IsEnabled = true;
                        textBlockL2penalty.Opacity = 0.5;
                        textBoxL2penalty.IsEnabled = false;
                    }
                    break;

                case DNNOptimizers.AdaGrad:
                    {
                        textBlockMomentum.Opacity = 0.5;
                        textBoxMomentum.IsEnabled = false;
                        textBlockL2penalty.Opacity = 0.5;
                        textBoxL2penalty.IsEnabled = false;
                    }
                    break;

                case DNNOptimizers.AdaBoundW:
                case DNNOptimizers.AdamW:
                case DNNOptimizers.AmsBoundW:
                case DNNOptimizers.NAG:
                case DNNOptimizers.SGDMomentum:
                case DNNOptimizers.SGDW:
                    {
                        textBlockMomentum.Opacity = 1;
                        textBoxMomentum.IsEnabled = true;
                        textBlockL2penalty.Opacity = 1;
                        textBoxL2penalty.IsEnabled = true;
                    }
                    break;
            }

            switch (optimizer)
            {
                case DNNOptimizers.AdaDelta:
                case DNNOptimizers.AdaGrad:
                case DNNOptimizers.NAG:
                case DNNOptimizers.SGD:
                case DNNOptimizers.SGDMomentum:
                case DNNOptimizers.SGDW:
                    {
                        textBlockBeta2.Opacity = 0.5;
                        textBoxBeta2.IsEnabled = false;
                    }
                    break;

                case DNNOptimizers.AdaBound:
                case DNNOptimizers.AdaBoundW:
                case DNNOptimizers.Adam:
                case DNNOptimizers.AdamW:
                case DNNOptimizers.Adamax:
                case DNNOptimizers.AmsBound:
                case DNNOptimizers.AmsBoundW:
                case DNNOptimizers.RMSProp:
                    {
                        textBlockBeta2.Opacity = 1;
                        textBoxBeta2.IsEnabled = true;
                    }
                    break;
            }

            switch (optimizer)
            {
                case DNNOptimizers.AdaDelta:
                case DNNOptimizers.AdaGrad:
                case DNNOptimizers.Adam:
                case DNNOptimizers.AdamW:
                case DNNOptimizers.Adamax:
                case DNNOptimizers.RMSProp:
                case DNNOptimizers.NAG:
                case DNNOptimizers.SGD:
                case DNNOptimizers.SGDMomentum:
                case DNNOptimizers.SGDW:
                    {
                        textBlockFinalLR.Opacity = 0.5;
                        textBoxFinalRate.IsEnabled = false;
                        textBlockGamma.Opacity = 0.5;
                        textBoxGamma.IsEnabled = false;
                    }
                    break;

                case DNNOptimizers.AdaBound:
                case DNNOptimizers.AdaBoundW:
                case DNNOptimizers.AmsBound:
                case DNNOptimizers.AmsBoundW:
                    {
                        textBlockFinalLR.Opacity = 1;
                        textBoxFinalRate.IsEnabled = true;
                        textBlockGamma.Opacity = 1;
                        textBoxGamma.IsEnabled = true;
                    }
                    break;
            }
        }

        private void ButtonSGDRHelp_Click(object sender, RoutedEventArgs e)
        {
            ApplicationHelper.OpenBrowser("https://arxiv.org/abs/1608.03983");
        }
    }
}
