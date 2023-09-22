namespace RadioDoge
{
    public class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            SerDogeSharp program = new SerDogeSharp();
            program.Execute();
        }
    }
}
