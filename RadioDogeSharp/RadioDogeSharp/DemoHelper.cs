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
            PrintTestTitle("LibDogecoin Address Generation Test");
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

        private void TestGeneratedAddressRegistrationAndRemoval()
        {
            PrintTestTitle("Generated Address Registration and Removal Test");

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

            // Once registered we will now remove the address
            Console.WriteLine($"Removing Registration: {publickey}");
            bool success = LibDogecoin.UnregisterWatchAddress(publickey);
            if (success)
            {
                Console.WriteLine($"Unregistration of generated address {publickey} successful!");
            }
            else
            {
                Console.WriteLine($"ERROR: Failed to unregister generated address {publickey}");
            }
            Console.WriteLine("");
        }

        private void TestFixedAddressRegistration()
        {
            PrintTestTitle("Fixed Address Registration Test");
            string address = "DKEXkLR3Q4w9q7CXGHm5GLMWSJPLqB8e53";
            RegistrationHelper(address);
        }

        /// <summary>
        /// Tests out LibDogecoin unregistration functions
        /// </summary>
        private void TestFixedAddressRemoval()
        {
            PrintTestTitle("Fixed Address Registration Removal Test");
            string unregisterAddress = "DKEXkLR3Q4w9q7CXGHm5GLMWSJPLqB8e53";
            //string unregisterAddress = testAddresses[0];
            bool success = LibDogecoin.UnregisterWatchAddress(unregisterAddress);
            if (success)
            {
                Console.WriteLine($"Unregistration of {unregisterAddress} successful!");
            }
            else
            {
                Console.WriteLine($"ERROR: Failed to unregister {unregisterAddress}");
            }
            Console.WriteLine("");
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
            Console.WriteLine();
        }

        private void TestBalanceInquiry()
        {
            PrintTestTitle("Balance Inquiry Test");
            for (int i = 0; i < testAddresses.Length; i++)
            {
                string currTestAddress = testAddresses[i];
                Console.WriteLine($"Getting Balance for Address: {currTestAddress}");
                UInt64 value = LibDogecoin.GetBalance(currTestAddress);
                Console.WriteLine($"Balance Value: {value}");
                string balanceString = LibDogecoin.GetBalanceString(currTestAddress);
                Console.WriteLine($"Balance String: {balanceString}\n");
            }
        }

        private void TestKoinuConversion()
        {
            // Test converting koinu amount to string
            PrintTestTitle("Koinu to Coins Conversion Test");
            UInt64 testBalance = 123456789;
            bool result = LibDogecoin.ConvertKoinuAmountToString(testBalance, out string convertedString);
            Console.WriteLine($"Original Test amount: {testBalance} (Koinu)");
            Console.WriteLine($"Converted Test amount: {convertedString}\n");
        }

        private bool CheckForWalletAndHeaderFiles()
        {
            PrintTestTitle("Wallet and Header File Test");
            bool verdict = true;
            string walletFilename = "main_wallet.db";
            string headersFilename = "main_headers.db";
            Console.WriteLine("Checking for database files...");
            //Check for the wallet file first
            if (!File.Exists(walletFilename))
            {
                Console.WriteLine($"Unable to find {walletFilename}");
                verdict = false;
            }
            else
            {
                Console.WriteLine($"Wallet file {walletFilename} found!");
            }

            if (!File.Exists(headersFilename))
            {
                Console.WriteLine($"Unable to find {headersFilename}");
                verdict = false;
            }
            else
            {
                Console.WriteLine($"Headers file {headersFilename} found!");
            }

            return verdict;
        }

        private void TestGetUTXOs()
        {
            PrintTestTitle("UTXO Test");
            string border = "-----------------------------";
            Console.WriteLine(border);
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
                Console.WriteLine(border);
            }
        }

        private void PrintTestTitle(string testTitle)
        {
            string border = "########################################################";
            int borderLength = border.Length - 2;
            Console.WriteLine(border);
            StringBuilder titleBuilder = new StringBuilder();
            int titleLength = testTitle.Length;
            int sideLength = (borderLength - titleLength) / 2;
            titleBuilder.Append("#");
            for (int i = 0; i < sideLength; i++)
            {
                titleBuilder.Append(" ");
            }
            titleBuilder.Append(testTitle);
            for (int i = 0; i < sideLength; i++)
            {
                titleBuilder.Append(" ");
            }

            // Add in another space if there is an odd number of characters in the title
            if (titleLength % 2 == 1)
            {
                titleBuilder.Append(" ");
            }
            titleBuilder.Append("#");
            Console.WriteLine(titleBuilder.ToString());
            Console.WriteLine(border);
        }

        private void LibdogecoinFunctionalityTesting()
        {
            TestAddressGeneration();
            TestGeneratedAddressRegistrationAndRemoval();
            TestFixedAddressRegistration();
            TestFixedAddressRemoval();
            TestBalanceInquiry();
            TestKoinuConversion();
            TestGetUTXOs();
            CheckForWalletAndHeaderFiles();
        }
    }
}
