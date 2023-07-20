using System.Text;

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

        private void TestAddressGeneration()
        {
            Console.WriteLine("Testing DogeCoin Library...");
            string privatekey;
            string publickey;

            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            LibDogecoin.dogecoin_ecc_start();
            int successReturn = LibDogecoin.generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successReturn == 1)
            {
                privatekey = pvkey.ToString();
                publickey = pubkey.ToString();

                Console.WriteLine("Generated test address info:");
                Console.WriteLine($"Test Private key: {privatekey}");
                Console.WriteLine($"Test Public key: {publickey}");
            }

            LibDogecoin.dogecoin_ecc_stop();
            Console.WriteLine("Dogecoin library test successful!\n");
        }

        private void TestAddressRegistration()
        {
            // Generate a test address
            Console.WriteLine("Testing Newly Generated Address Registration...");

            // First we generate an address
            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            LibDogecoin.dogecoin_ecc_start();
            int successReturn = LibDogecoin.generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successReturn != 1)
            {
                Console.WriteLine("Failure to generate address!");
                return;
            }
            LibDogecoin.dogecoin_ecc_stop();

            // Now we try and register it
            string publickey = pubkey.ToString();
            Console.WriteLine($"Registering Address: {publickey}");
            RegistrationHelper(publickey);
        }

        private void TestFixedAddressRegistration()
        {
            Console.WriteLine("Testing Fixed Address Registration...");
            string address = "DKEXkLR3Q4w9q7CXGHm5GLMWSJPLqB8e53";
            RegistrationHelper(address);
        }

        /// <summary>
        /// Tests out LibDogecoin unregistration functions
        /// </summary>
        private void TestAddressRemoval()
        {
            Console.WriteLine("Testing Unregistration of Address...");
            string unregisterAddress = "";
            bool success = LibDogecoin.UnregisterWatchAddress(unregisterAddress);
            if (success)
            {
                Console.WriteLine($"Unregistration of {unregisterAddress} successful!");
            }
            else
            {
                Console.WriteLine($"ERROR: Failed to unregister {unregisterAddress}");
            }
        }

        private void RegistrationHelper(string address)
        {
            Console.WriteLine($"Attempting to register address: {address}");
            bool registrationSuccess = LibDogecoin.RegisterWatchAddress(address);
            if (registrationSuccess)
            {
                Console.WriteLine($"Successfully registered address! {address}");
            }
            else
            {
                Console.WriteLine($"Failed to register address! {address}");
            }
        }

        private void TestBalanceInquiry()
        {
            Console.WriteLine("Balance Inquiry Test");
            for (int i = 0; i < testAddresses.Length; i++)
            {
                string currTestAddress = testAddresses[i];
                Console.WriteLine($"Getting Balance for Address: {currTestAddress}");
                UInt64 value = LibDogecoin.GetBalance(currTestAddress);
                Console.WriteLine($"Balance Value: {value}");
                string balanceString = LibDogecoin.GetBalanceString(currTestAddress);
                Console.WriteLine($"Balance String: {balanceString}");
            }
        }

        private void TestKoinuConversion()
        {
            // Test converting koinu amount to string
            Console.WriteLine("\nTesting Koinu to Coins conversion...");
            UInt64 testBalance = 123456789;
            bool result = LibDogecoin.ConvertKoinuAmountToString(testBalance, out string convertedString);
            Console.WriteLine($"Original Test amount: {testBalance} (Koinu)");
            Console.WriteLine($"Converted Test amount: {convertedString}\n");
        }

        private void TestGetUTXOs()
        {
            for (int i = 0; i < testAddresses.Length; i++)
            {
                string address = testAddresses[i];
                Console.WriteLine($"Testing getting UTXOs for {address}");
                UInt32 numUTXOs = LibDogecoin.GetNumberOfUTXOs(address);
                Console.WriteLine($"Found {numUTXOs} UTXOs for {address}");
                if (numUTXOs > 0)
                {
                    byte[] serializedUTXOs = LibDogecoin.GetAllSerializedUTXOs(numUTXOs, address);
                    string utxoHex = Convert.ToHexString(serializedUTXOs);
                    Console.WriteLine($"Serialized UTXOs: {utxoHex}");
                }
                else
                {
                    Console.WriteLine($"No UTXOs found for {address}");
                }
                Console.WriteLine("\n");
            }
        }

        private void LibdogecoinFunctionalityTesting()
        {
            TestAddressGeneration();
            //TestFixedAddressRegistration();
            //TestAddressRegistration();
            TestAddressRemoval();
            TestBalanceInquiry();
            TestKoinuConversion();
            TestGetUTXOs();
        }
    }
}
