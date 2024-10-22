#include <algorithm> 
#include <chrono>    
#include <fstream>   
#include <iostream>
#include <map>
#include <mutex> 
#include <set>   
#include <sstream>
#include <thread> 
#include <vector>

using namespace std;

// Function to trim leading and trailing spaces
std::string trim(const std::string& str) {
    // Find the first non-space character
    size_t start = str.find_first_not_of(' ');
    // If the string is all spaces, return an empty string
    if (start == std::string::npos) {
        return "";
    }
    // Find the last non-space character
    size_t end = str.find_last_not_of(' ');
    // Return the substring with leading/trailing spaces removed
    return str.substr(start, end - start + 1);
}

// Global mutex for synchronizing console output
mutex printMutex;
int totalProcesses = 10;

bool isAlphanumeric(const std::string& str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), [](char c) {
      return std::isalnum(c) || c == '_'; // Allowing underscores as well
  });
}

bool checkInstructions(std::ifstream& inputFile) {
    std::string line;
    std::set<std::string> activeProcesses; // To track active processes

    while (getline(inputFile, line)) {
        line = trim(line);  // Trim leading and trailing spaces

        if (line.find("begin process") != std::string::npos) {
            // Extract process name
            std::string processName = line.substr(line.find("begin process") + 14);
            activeProcesses.insert(processName);
        } 
        else if (line.find("end process") != std::string::npos) {
            // Extract process name
            std::string processName = line.substr(line.find("end process") + 12);
            if (activeProcesses.find(processName) == activeProcesses.end()) {
                std::cerr << "Error: 'end process " << processName << "' without matching 'begin process'. Stopping execution." << std::endl;
                return false; // Error found
            }
            activeProcesses.erase(processName);
        } 
        else if (line.find("send") != std::string::npos) {
            // Extract and check message
            std::string message = line.substr(line.find("send") + 5);
            if (!isAlphanumeric(trim(message))) {
                std::cerr << "Error: Malformed 'send' instruction. Message must be alphanumeric. Stopping execution." << std::endl;
                return false; // Error found
            }
        } 
        else if (line.find("recv_B") != std::string::npos) {
            // Check the 'recv_B' instruction for correct format
            std::string recvInstruction = line.substr(line.find("recv_B") + 7);
            if (recvInstruction.find(' ') == std::string::npos) {
                std::cerr << "Error: Malformed 'recv_B' instruction. Must have two arguments. Stopping execution." << std::endl;
                return false; // Error found
            }
        } 
        else {
            // Unrecognized instruction
            std::cerr << "Error: Unrecognized instruction: '" << line << "'. Stopping execution." << std::endl;
            return false; // Error found
        }
    }

    // Check for unmatched 'begin process'
    if (!activeProcesses.empty()) {
        std::cerr << "Error: Unmatched 'begin process' for processes: ";
        for (const auto& process : activeProcesses) {
            std::cerr << process << " ";
        }
        std::cerr << ". Stopping execution." << std::endl;
        return false; // Error found
    }

    return true; // No errors found
}

// Vector clock class
class VectorClock {
public:
  vector<int> clock;

  VectorClock(int n) : clock(n, 0) {}

  void increment(int process) { clock[process]++; }

  void update(const vector<int> &other) {
    for (int i = 0; i < clock.size(); i++) {
      clock[i] = max(clock[i], other[i]);
    }
  }

  bool isAllowed(const VectorClock &other, int thisid, int fromid) {
    lock_guard<mutex> lock(printMutex);
    cout << "Is allowed called by p" << thisid + 1 << " for "
         << this->toString() << " " << other.toString() << " from p"
         << fromid + 1 << endl;
    for (int i = 0; i < clock.size(); i++) {
      if (i == fromid) {
        if (clock[i] + 1 != other.clock[i])
          return false;
        continue;
      }
      if (clock[i] < other.clock[i])
        return false;
    }
    return true;
  }

// Custom comparison operator for VectorClock
bool operator<(const VectorClock &other) const {
    return clock < other.clock; // Use vector lexicographical comparison
}

  string toString() const {
    stringstream ss;
    ss << "(";
    for (int i = 0; i < clock.size(); i++) {
      ss << clock[i];
      if (i < clock.size() - 1)
        ss << ",";
    }
    ss << ")";
    return ss.str();
  }
};

// Message structure (modified to store integer instead of string)
struct Message {
  string msg; // Storing integer instead of string
  int fromProcess;
  VectorClock vc;

  Message(string msg, int fromProcess, const VectorClock &vc)
      : msg(msg), fromProcess(fromProcess), vc(vc) {}

  // Custom comparator to compare messages based on their vector clocks
  bool operator<(const Message &other) const {
    return vc < other.vc; // Higher vector clocks come later in the set
  }
};
// Add mutexes for the message queues in the Process class
class Process {
public:
  int id;
  VectorClock vc;
  vector<string> operations;
  multiset<Message>
      messageQueue; // Set for messages received by BSS, sorted by vector clocks
  multiset<Message> appMessageQueue; // Set for messages to be read by Application
  vector<string> outputOperations;

  mutex messageQueueMutex;    // Mutex for messageQueue
  mutex appMessageQueueMutex; // Mutex for appMessageQueue

  Process(int id, int totalProcesses) : id(id), vc(totalProcesses) {}

  void sendMessage(string msg, vector<Process *> &processes) {
    vc.increment(id);             // Increment the vector clock for this process
    string vcStr = vc.toString(); // Construct the vector clock string
    string formattedMessage =
        "p" + to_string(id + 1) + " send " + msg + " " + vcStr;
    {
      lock_guard<mutex> lock(printMutex); // Synchronize output
      printf("%s\n", formattedMessage.c_str());
    }
    outputOperations.push_back(formattedMessage);

    // Broadcast to all processes
    for (Process *p : processes) {
      if (p->id != id) {
        lock_guard<mutex> lock(p->messageQueueMutex); // Lock messageQueue of the other process  
        p->messageQueue.insert(Message(msg, id, vc));
      }
    }
  }

  void bssRecvMessage(string msg, int fromProcess) {
    int it = 0;
    while (true) {
      {
        lock_guard<mutex> lock(printMutex); // Synchronize output
        cout << "For process p" << id + 1 << " messageQueue's size is "
             << messageQueue.size() << endl;
      }
      if (!messageQueue.empty()) {
        lock_guard<mutex> lock(
            messageQueueMutex); // Lock messageQueue while checking
        for (auto it = messageQueue.begin(); it != messageQueue.end(); ++it) {
          const Message &receivedMsg = *it;
          if (receivedMsg.msg == msg &&
              receivedMsg.fromProcess == fromProcess) {
            {
              lock_guard<mutex> lock(printMutex); // Synchronize output
              string recvBOperation = "recv_B p" +
                                      to_string(receivedMsg.fromProcess + 1) +
                                      " " + (msg) + " " + vc.toString();
              outputOperations.push_back(recvBOperation); // Log the operation
              cout << "p" << id + 1 << " " << recvBOperation << endl;
            }

            {
              lock_guard<mutex> lock(
                  appMessageQueueMutex); // Lock appMessageQueue
              appMessageQueue.insert(receivedMsg);
            }

            // Erase the received message from the queue
            messageQueue.erase(it);
            return;
          }
        }
      }
      it++;
      {
        lock_guard<mutex> lock(printMutex);
        if (it > 2 * totalProcesses) {
          std::cout << "Inconsistent Input Detected" << std::endl;
          exit(EXIT_FAILURE);
        }
        // Block until the message arrives, simulating asynchronous message
        // delivery
        cout << "p" << id + 1 << " is sleeping..." << endl;
      }
      this_thread::sleep_for(chrono::milliseconds(500));
    }
  }

  void appRecvMessage() {
    while (!appMessageQueue.empty()) {
      lock_guard<mutex> lock(
          appMessageQueueMutex); // Lock appMessageQueue while accessing
      auto it = appMessageQueue.begin();
      const Message &receivedMsg = *it;
      if (vc.isAllowed(receivedMsg.vc, id, receivedMsg.fromProcess)) {
        vc.update(receivedMsg.vc.clock); // Update vector clock
        {
          lock_guard<mutex> lock(printMutex); // Synchronize output
          string recvAOperation = "recv_A p" +
                                  to_string(receivedMsg.fromProcess + 1) + " " +
                                  (receivedMsg.msg) + " " + vc.toString();
          outputOperations.push_back(recvAOperation); // Log the operation
          cout << "p" << id + 1 << " " << recvAOperation << endl;
        }
        appMessageQueue.erase(
            it); // Remove the message from the appMessageQueue
      } else
        return;
    }
  }

  void processOperations(vector<Process *> &processes) {
    for (const string &operation : operations) {
      {
        lock_guard<mutex> lock(printMutex);
        cout << "p" << id + 1 << " reached op: " << operation << endl;
      }
      if (operation.find("send") != string::npos) {
        string msg = trim(operation.substr(5)); // Convert message to integer
        sendMessage(msg, processes);
      } else if (operation.find("recv_B") != string::npos) {
        stringstream ss(operation);
        string recvType, fromProcessStr, msgStr;
        ss >> recvType >> fromProcessStr >> msgStr;
        int fromProcess =
            fromProcessStr[1] - '0'; // Convert process number to index
        bssRecvMessage(msgStr, fromProcess - 1);
        appRecvMessage();
      }
    }
  }

  void addOperation(const string &operation) {
    operations.push_back(operation);
  }
};
// Main function to run all processes
int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " <input_file>" << endl;
    return 1;
  }

  ifstream inputFile(argv[1]);
  if (!inputFile.is_open()) {
    cerr << "Error: Could not open file " << argv[1] << endl;
    return 1;
  }

  string line;

  vector<Process *> processes;
  vector<thread> threads;

  // Initialize processes
  totalProcesses = 0;
  while (getline(inputFile, line)) {
    if (line.find("begin process") != std::string::npos) {
      totalProcesses++;
    }
  }
  cout << "The Total Processes are " << totalProcesses << endl;
  for (int i = 0; i < totalProcesses; i++) {
    processes.push_back(new Process(i, totalProcesses));
  }
  inputFile.clear();  // clear the EOF flag
  inputFile.seekg(0); // move to the beginning of the file

  // Check for errors
  if (!checkInstructions(inputFile)) {
      return 1; // Stop execution due to errors
  }
  inputFile.clear();
  inputFile.seekg(0);
  
  // Parse input
  while (getline(inputFile, line)) {
    if (line.find("begin process") != string::npos) {
      int processId = stoi(line.substr(15)) - 1;
      Process *p = processes[processId];
      while (getline(inputFile, line) &&
             line.find("end process") == string::npos) {
        p->addOperation(line);
      }
    }
  }

  inputFile.close();

  // Launch threads for each process
  for (Process *p : processes) {
    threads.emplace_back(&Process::processOperations, p, ref(processes));
  }

  // Wait for all threads to complete
  for (auto &t : threads) {
    t.join();
  }

  // Clean up and output
  ofstream outFile("output.txt"); // Create and open the output file
  for (Process *p : processes) {
    if (outFile.is_open()) {
      // Write the operations of the process to the file
      outFile << "begin process p" << p->id + 1 << endl;
      for (const string &op : p->outputOperations) {
        outFile << op << "\n"; // Write each operation to the file
      }
      outFile << "end process p" << p->id + 1
              << "\n \n"; // Newline for separation between processes
    }
    delete p; // Free the memory allocated for the process
  }

  outFile.close();

  return 0;
}