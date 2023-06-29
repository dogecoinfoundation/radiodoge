namespace RadioDoge
{
    /// <summary>
    /// Abstract class used for helping to implement the different modes
    /// </summary>
    internal abstract class RadioDogeMode
    {
        public abstract void ProcessCommand(int commandValue);

        public abstract void PrintModeHelp();
    }
}
