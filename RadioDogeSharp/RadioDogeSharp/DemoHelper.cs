namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private NodeAddress demoHostAddress = new NodeAddress(10, 0, 1);
        private NodeAddress demoDestinationAddress = new NodeAddress(10, 0, 2);
        
        private void DemoNodeSetup()
        {
            Console.WriteLine($"Setting up node address: {demoHostAddress.ToString()}");
            // Set local node address and destination address
            CommandSetNodeAddress(demoHostAddress);
            // Assign demo destination address as the destination
            destinationAddress = demoDestinationAddress;
            Thread.Sleep(2500);
        }

        private void LibdogecoinFunctionalityTesting()
        {
            LibDogecoin.DogeTest();
            TestBalanceInquiry();
            TestKoinuConversion();
            TestGetUTXOs();
        }
    }
}
