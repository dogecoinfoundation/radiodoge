using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace RadioDoge
{
    public static class LibDogecoin
    {
        private const string libToImport = "dogecoin";

        public static void DogeTest()
        {
            Console.WriteLine("Testing DogeCoin Library...");
            string privatekey;
            string publickey;

            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            dogecoin_ecc_start();
            int successret = generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successret == 1)
            {
                privatekey = pvkey.ToString();
                publickey = pubkey.ToString();

                Console.WriteLine("Generated test address info:");
                Console.WriteLine($"Private key: {privatekey}");
                Console.WriteLine($"Public key: {publickey}");
            }

            dogecoin_ecc_stop();
            Console.WriteLine("Dogecoin library test successful!\n");
        }
        // @TODO review choice of types used
        // size_t -> UIntPtr
        // const char* -> string
        // uint8_t* -> byte[]
        [DllImport(libToImport)]
        public static extern uint dogecoin_ecc_start();

        [DllImport(libToImport)]
        public static extern void dogecoin_ecc_stop();

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int generatePrivPubKeypair(StringBuilder wif_privkey, StringBuilder p2pkh_pubkey, int testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int generateHDMasterPubKeypair(StringBuilder wif_privkey_master, StringBuilder p2pkh_pubkey_master, bool is_testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int generateDerivedHDPubkey(string wif_privkey_master, StringBuilder p2pkh_pubkey);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int verifyPrivPubKeypair(StringBuilder wif_privkey, StringBuilder p2pkh_pubkey, bool is_testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int verifyHDMasterPubKeypair(StringBuilder wif_privkey_master, StringBuilder p2pkh_pubkey_master, bool is_testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int verifyP2pkhAddress(StringBuilder p2pkh_pubkey, UIntPtr len);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int getDerivedHDAddress(string masterkey, UInt32 account, bool ischange, UInt32 addressindex, StringBuilder outaddress, bool outprivkey);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int getDerivedHDAddressByPath(string masterkey, string derived_path, StringBuilder outaddress, bool outprivkey);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int dogecoin_register_watch_address_with_node(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int dogecoin_unregister_watch_address_with_node(string address);

        /*
        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int dogecoin_get_utxo_vector(string address, vector* utxos);
        */

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern byte[] dogecoin_get_utxos(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt32 dogecoin_get_utxos_length(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr dogecoin_get_utxo_txid_str(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern byte[] dogecoin_get_utxo_txid(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern UInt64 dogecoin_get_balance(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr dogecoin_get_balance_str(string address);
    }
}




