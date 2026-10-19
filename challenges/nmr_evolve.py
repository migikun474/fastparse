import jax
import jax.numpy as jnp
import json
import pennylane as qp
# Write any helper functions you need here

dev = qp.device('default.qubit', wires = 1)

@qp.qnode(dev)
def nmr_evolve(B0: float, B1: float, omega: float, time: float) -> "jnp.ndarray":
    """This quantum circuit evolves the initial state |0> under the evolution operator generated
    by the nuclear magnetic resonance Hamiltonian.

    Args:
        B0 (float): Coefficient of the Pauli Z term in the Hamiltonian.
        B1 (float): Coefficient of the time-dependent component of the Hamiltonian.
        omega (float): Magnetic field oscillation frequency.
        time (float): Period of time during which the magnetic field acts on the qubit.

    Returns:
        (numpy.array): The probabilities of measuring each computational basis state.
    """

    # Put your code here #
    # H(t) = B0 Z + B1 cos(wt) X - B1 sin(wt) Y; callable coefficients take (p, t)
    def f_x(p, t):
        return p * jnp.cos(omega * t)

    def f_y(p, t):
        return -p * jnp.sin(omega * t)

    H = B0 * qp.Z(0) + f_x * qp.X(0) + f_y * qp.Y(0)
    qp.evolve(H)([B1, B1], t=time)

    # Return the probabilities
    return qp.probs(wires=0)

# These functions are responsible for testing the solution.
def run(test_case_input: str) -> str:
    B0, B1, omega, t = json.loads(test_case_input)
    return str([float(p) for p in nmr_evolve(B0, B1, omega, t)])


test_cases = [
    ('[1,2,0.5,0.3]', [0.6962061524391174, 0.3037952184677124]),
    ('[1, 1, 0.25, 0.6]', [0.7278291583061218, 0.272170752286911]),
]

if __name__ == "__main__":
    for i, (inp, expected) in enumerate(test_cases, 1):
        out = json.loads(run(inp))
        ok = all(abs(o - e) < 3e-2 for o, e in zip(out, expected))
        print(f"Test {i}: {out} -> {'Success!' if ok else 'Incorrect'}")
