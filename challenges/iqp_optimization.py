import json
import pennylane as qp
import pennylane.numpy as np
# Write any helper functions you need here

n_qubits = 4

dev = qp.device('default.qubit', wires = n_qubits)

@qp.qnode(dev)
def iqp_circuit(weights, pattern, observable):

    """This quantum circuit implements the IQP circuit with the given weight, pattern, and observable
    Args:
        weights (np.array(float)): The parameters of the IQP circuit.
        pattern (float): The pattern of the rotation groups as per challenge description.
        observable (np.array(int)): The array describing the observable as per challenge description

    Returns:
        (numpy.array): The expectation value of the observable for the prepared IQP state.
    """


    # Put your code here #
    # Each group shares one parameter; PauliRot(theta, P) = exp(-i theta/2 P)
    for theta, group in zip(weights, pattern):
        for subset in group:
            subset = [int(w) for w in subset]
            qp.PauliRot(theta, "X" * len(subset), wires=subset)

    # Return the expectation value
    wires = [k for k, a in enumerate(observable) if int(a) == 1]
    if not wires:
        return qp.expval(qp.Identity(0))
    return qp.expval(qp.prod(*[qp.Z(k) for k in wires]))

def optimize_iqp(pattern, observable):

    """This function finds the weights that optimize the IQP circuit built above
    Args:
        pattern (float): The pattern of the rotation groups as per challenge description.
        observable (np.array(int)): The array describing the observable as per challenge description

    Returns:
        (numpy.array): The expectation value of the observable for the prepared IQP state.
    """


    # Put your code here #
    def cost(w):
        return iqp_circuit(w, pattern, observable)

    # theta = 0 is a stationary point (<Z_a> = 1), so start from random points
    # and keep the best result over a few restarts.
    np.random.seed(42)
    best_w, best_c = None, np.inf
    for _ in range(8):
        w = np.array(np.random.uniform(0, 2 * np.pi, len(pattern)), requires_grad=True)
        opt = qp.AdamOptimizer(stepsize=0.1)
        for _ in range(200):
            w = opt.step(cost, w)
        c = cost(w)
        if c < best_c:
            best_w, best_c = w, c
        if best_c < -1 + 1e-4:
            break

    # Return the optimized weights
    return np.array(best_w, requires_grad=False)

# These functions are responsible for testing the solution.
def run(test_case_input: str) -> str:
    pattern, observable = json.loads(test_case_input)
    weights = optimize_iqp(pattern, observable)
    return str([float(iqp_circuit(weights, pattern, observable))])


test_cases = [
    ('[[[[0], [1]], [[1, 2]], [[0, 2], [1, 3]], [[0, 1, 2, 3]]], [1, 0, 1, 0]]', -1),
    ('[[[[0], [2]], [[1, 2, 3]], [[0, 1], [2, 3]], [[0, 1, 2]]], [1, 1, 0, 1]]', -1),
]

if __name__ == "__main__":
    for i, (inp, expected) in enumerate(test_cases, 1):
        out = json.loads(run(inp))[0]
        status = "Success!" if abs(out - expected) < 2e-2 else "Incorrect"
        print(f"Test {i}: cost={out:.6f} -> {status}")
